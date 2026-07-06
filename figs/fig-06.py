import matplotlib.pyplot as plt
import numpy as np
import matplotlib as mpl
import os
import sys

plt.rcParams['axes.labelsize'] = 15  
plt.rcParams['xtick.labelsize'] = 13
plt.rcParams['ytick.labelsize'] = 13
mpl.rcParams['pdf.fonttype'] = 42
plt.rcParams['font.sans-serif'] = ['Arial']
plt.rcParams["figure.figsize"] = (16, 9)

def obter_janela_valida(arquivo_rps):
    """
    Le o arquivo de RPS e retorna:
    - rps_primeiro_segundo: quantidade de reqs no segundo inicial.
    - rps_ultimo_segundo: quantidade de reqs no segundo final.
    """
    rps_por_segundo = []
    if not os.path.exists(arquivo_rps):
        print(f"Error: RPS file not found -> {arquivo_rps}")
        return 0, 0

    rps_10sec = 0
    with open(arquivo_rps, 'r') as f:
        for line in f:
            partes = line.split()
            if len(partes) >= 2:
                # partes[1] é a quantidade de RPS naquele segundo
                qtd_rps = int(partes[1])
                rps_por_segundo.append(qtd_rps)

    if len(rps_por_segundo) < 2:
        return 0, 0

    for i in range(0, 10):
        rps_10sec = rps_10sec + rps_por_segundo[i]

    return rps_10sec, rps_por_segundo[-1]

###################################################################################
def coleta_dados_sincronizados(file_latencia, arquivo_rps):
    """Le latencias e remove o início e fim baseado no RPS."""
    rps_inicio, rps_fim = obter_janela_valida(arquivo_rps)
    
    todas_latencias = []
    if not os.path.exists(file_latencia):
        print(f"Error: Latency file not found -> {file_latencia}")
        return []

    with open(file_latencia, 'r') as f:
        for line in f:
            partes = line.split()
            if partes:
                try:
                    # Converte o último valor (latência) para ms
                    valor = float(partes[-1]) / 1000
                    todas_latencias.append(valor)
                except ValueError:
                    continue

    total_lido = len(todas_latencias)
    
    # Logica de descarte:
    # O fatiamento [rps_inicio : -rps_fim] faz exatamente isso
    if total_lido > (rps_inicio + rps_fim):
        dados_filtrados = todas_latencias[rps_inicio : -rps_fim]
    else:
        print(f"Warning: Unsuficient samples in {file_latencia} to discard begin/end.")
        dados_filtrados = []

    print(f"File: {os.path.basename(file_latencia)}")
    print(f"  - Total latencies read: {total_lido}")
    print(f"  - Discarded from begining (RPS sec 10): {rps_inicio}")
    print(f"  - Discarded from end (RPS last sec): {rps_fim}")
    print(f"  - Remaining samples to CDF: {len(dados_filtrados)}")
    
    return dados_filtrados

###################################################################################
def plotar_cdf_99th(dados1, label1, dados2, label2, rps_alvo):

    if not dados1 or not dados2:
        print(f"Data not fount to RPS {rps_alvo}")
        return

    for dados, nome, cor in [(dados1, label1, 'tab:blue'), (dados2, label2, 'tab:red')]:
        # Ordena os dados para a CDF
        dados_sorted = np.sort(dados)
        
        # Filtra para o 99º percentil (remove os 1% maiores outliers para focar na cauda relevante)
        #p99_limit = np.percentile(dados_sorted, 99)
        max_percent = 99
        p99_limit = np.percentile(dados_sorted, max_percent)
        dados_filtrados = dados_sorted[dados_sorted <= p99_limit]
        
        # Calcula a probabilidade acumulada
        y = np.arange(1, len(dados_filtrados) + 1) / len(dados_filtrados)
        plt.plot(dados_filtrados, y, label=f"{nome} (P{max_percent}: {p99_limit:.2f} ms)", color=cor, linewidth=7)

    plt.annotate('Lower Latency is Better', xy=(1.05, 1.0), xytext=(1.25, 1.0), fontsize=20, arrowprops=dict(facecolor='yellow', shrink=0.05))

    #plt.title(f'Comparative CDF (Until P{max_percent}) - Load: {rps_alvo} k', fontsize=35)
    plt.xlabel('Latency (ms)', fontsize=35)
    plt.ylabel('Cumulative Probability', fontsize=35)
    plt.grid(True, linestyle='--', linewidth=1)

    plt.xticks(fontsize=45)
    plt.yticks(fontsize=45)

    prop = dict(size=25)
    plt.legend(loc='lower right', ncol = 2, prop = prop, borderaxespad = 0, frameon = True, columnspacing = 0.7, labelspacing = 0.05, fancybox = False, edgecolor = 'black')
   
    file_saved = rps_alvo + "k_99th.png"
    plt.tight_layout()
    plt.savefig( file_saved ,pad_inches=0.01)
    #plt.show()

###################################################################################
if __name__ == '__main__':
    
    if len(sys.argv) < 2:
        print("Uso: python script.py <RPS>")
        sys.exit(1)

    path_sig = "./sigbpf/lat/"
    path_sp = "./spright/lat/"
    RPS = sys.argv[1] 

    # --- FILES PATH ---
    lat_sig = path_sig + "latencies_"     + RPS + "000rps-mod.txt"
    rps_sig = path_sig + "requests_recv_" + RPS + "000rps-mod.txt"
    
    lat_sp  = path_sp + "latencies_" +     RPS + "000rps-mod.txt"
    rps_sp  = path_sp + "requests_recv_" + RPS + "000rps-mod.txt"

    print("=== Processing Sigbpf ===")
    dados_sig = coleta_dados_sincronizados(lat_sig, rps_sig)
    
    print("\n=== Processing Spright ===")
    dados_sp = coleta_dados_sincronizados(lat_sp, rps_sp)

    plotar_cdf_99th(dados_sig, "Sigbpf " + RPS + " k", dados_sp, "Spright " + RPS + " k", RPS)
