import os
import re
import matplotlib.pyplot as plt
import numpy as np


def extrair_valor_rps(texto):
    """Converte '12K' ou '12000' em 12000 para ordenação."""
    match = re.search(r'(\d+)([kK]?)', texto)
    if match:
        valor = int(match.group(1))
        unidade = match.group(2).lower()
        return valor * 1000 if unidade == 'k' else valor
    return 0


def obter_descartes_rps(caminho_rps):
    if not os.path.exists(caminho_rps): 
        return 0, 0
    rps_valores = []
    rps_10sec = 0
    with open(caminho_rps, 'r') as f:
        for line in f:
            parts = line.split()
            if len(parts) >= 2:
                try:
                    # Coleta o valor numérico da segunda coluna
                    rps_valores.append(int(parts[1]))
                except ValueError:
                    continue
    if not rps_valores: 
        print(f"Valores ignorados: inicio(0) fim(0)")
        return 0, 0

    for i in range(0, min(10, len(rps_valores))):
        rps_10sec = rps_10sec + rps_valores[i] 

    return rps_10sec, rps_valores[-1]


def coleta_dados_sincronizados(caminho_lat, caminho_rps):
    rps_inicio, rps_fim = obter_descartes_rps(caminho_rps)

    latencias = []
    with open(caminho_lat, 'r') as fp:
        for line in fp:
            parts = line.strip().split()
            if len(parts) >= 2:
                try:
                    # Converte o valor de latência da segunda coluna
                    latencias.append(float(parts[1]) / 1000)
                except ValueError: 
                    continue
    if rps_fim > 0:
        return rps_inicio, rps_fim, latencias[rps_inicio : -rps_fim]
    return latencias[rps_inicio:]


def carregar_dados_diretos(diretorio_raiz):
    """Carrega dados considerando que os arquivos estão direto na pasta informada."""
    dados = {}
    if not os.path.exists(diretorio_raiz): 
        print(f"[ERRO] Caminho não encontrado: {diretorio_raiz}")
        return dados
    
    arquivos = os.listdir(diretorio_raiz)
    # Filtra arquivos de latência que começam com 'latencies_' e terminam com '.txt'
    lat_files = [f for f in arquivos if f.startswith("latencies_") and f.endswith(".txt")]
    
    for f_lat in lat_files:
        # Extrai o ID da carga (ex: latencies_10000-mod.txt -> grupo captura '10000')
        num_match = re.search(r'latencies_(.*?)(?:-mod)?\.txt', f_lat)
        if not num_match: 
            continue
        num_alvo = num_match.group(1)
        
        # Encontra o correspondente de requisições recebidas contendo o mesmo ID
        f_rps = next((f for f in arquivos if "requests_recv" in f and num_alvo in f), None)
        
        if f_rps:
            rps_inicio, rps_fim, res = coleta_dados_sincronizados(
                os.path.join(diretorio_raiz, f_lat), 
                os.path.join(diretorio_raiz, f_rps)
            )
            
            # Agrupa os resultados usando a chave identificadora extraída
            if res:
                if num_alvo not in dados:
                    dados[num_alvo] = []
                dados[num_alvo].extend(res)
                print(f"[OK] Reading: {f_lat} <-> {f_rps} | ignored: start->{rps_inicio} end->{rps_fim}")
        else:
            print(f"[Warning] File requests_recv not found to {f_lat}")
            
    return dados


def comparar_arquiteturas(path_arq1, path_arq2, nome1="Arq X", nome2="Arq Y", rps_max=None):
    print(f"== Reading SPRIGHT ==")
    dados1 = carregar_dados_diretos(path_arq1)
    print(f"== Reading SIGBPF ==")
    dados2 = carregar_dados_diretos(path_arq2)

    # Validação de dados carregados
    if not dados1:
        print(f"[ERROR] Data not found in: {path_arq1}")
    if not dados2:
        print(f"[ERROR] Data not found in: {path_arq2}")

    # Faz a intersecção de cargas comuns baseando-se estritamente nas chaves encontradas
    cargas_comuns = sorted(list(set(dados1.keys()) & set(dados2.keys())), key=extrair_valor_rps)
    if rps_max:
        cargas_comuns = [c for c in cargas_comuns if extrair_valor_rps(c) <= rps_max]

    if not cargas_comuns:
        print("\n[ERROR] Cant generate graph")
        print(f"Keys in {nome1}: {list(dados1.keys())}")
        print(f"Keys in {nome2}: {list(dados2.keys())}")
        return

    # CONVERSÃO DOS LABELS: Transforma valores do tipo '1000rps' ou '2000' para '1 k', '2 k', etc.
    labels_formatados = []
    for c in cargas_comuns:
        num_match = re.search(r'\d+', c)
        if num_match:
            num_puro = int(num_match.group())
            if num_puro >= 1000:
                labels_formatados.append(f"{num_puro // 1000} k")
            else:
                labels_formatados.append(str(num_puro))
        else:
            labels_formatados.append(c)

    plt.figure(figsize=(16, 9))
    indices = np.arange(len(cargas_comuns))
    largura = 0.35
   
    max_percent = 99.9
    flier = False 
    
    bp1 = plt.boxplot([dados1[c] for c in cargas_comuns], positions=indices - largura/2, widths=largura, patch_artist=True, whis=(0, max_percent), showfliers=flier)
    bp2 = plt.boxplot([dados2[c] for c in cargas_comuns], positions=indices + largura/2, widths=largura, patch_artist=True, whis=(0, max_percent), showfliers=flier)

    # Coleta a coordenada Y do topo do whisker superior de cada boxplot para a linha de tendência
    w1 = [w.get_ydata()[1] for w in bp1['whiskers'][1::2]]
    w2 = [w.get_ydata()[1] for w in bp2['whiskers'][1::2]]

    plt.plot(indices - largura/2, w1, color='darkred', linestyle='--', marker='o', label=f'Max {nome1}')
    plt.plot(indices + largura/2, w2, color='tab:blue', linestyle='--', marker='s', label=f'Max {nome2}')

    for box in bp1['boxes']: 
        box.set(facecolor='tab:red')
    for box in bp2['boxes']: 
        box.set(facecolor='lightblue')

    #title = f"Boxplot of {max_percent}% of the data with fliers={flier}"
    #plt.title(title, fontsize=25)
    
    # Aplica os novos labels abreviados no eixo X
    plt.xticks(indices, labels_formatados, fontsize=25)
    plt.yticks(fontsize=25)

    plt.ylabel('Latency (ms)', fontsize=30)
    plt.xlabel('Load (Req/s)', fontsize=30)
    plt.legend(fontsize=18)
    plt.grid(axis='y', linestyle='--', alpha=0.3)
    
    plt.tight_layout()
    plt.savefig("boxplot_RPS.png")
    #print("\n[SUCESSO] Gráfico 'boxplot_RPS.png' gerado com os labels abreviados!")


###########################################################################################
if __name__ == "__main__":
    comparar_arquiteturas('./spright/lat', './sigbpf/lat', "SPRIGHT", "SIGBPF", rps_max=11000)

