import numpy as np
import matplotlib.pyplot as plt
from matplotlib.patches import Polygon
import matplotlib as mpl
import os
import sys

# Configurações de estilo mantidas
plt.rcParams['font.sans-serif'] = ['Arial']
plt.rcParams["figure.figsize"] = (16, 9)

rps = 0

def obter_limites_rps(arquivo_rps):
    """Retorna o RPS do primeiro e do último segundo para descarte."""
    if not os.path.exists(arquivo_rps):
        print(f"Error: file RPS not found -> {arquivo_rps}")
        return 0, 0
    
    rps_valores = []
    with open(arquivo_rps, 'r') as f:
        for line in f:
            parts = line.split()
            if len(parts) >= 2:
                rps_valores.append(int(parts[1])) # Pega a 2ª coluna
    
    if not rps_valores: return 0, 0
    return rps_valores[0], rps_valores[-1]

def coleta_dados_filtrados(file_latencia, file_rps):
    """Coleta dados, aplica o descarte de início/fim e separa por categoria."""
    rps_inicio, rps_fim = obter_limites_rps(file_rps)
    
    todas_linhas = []
    if not os.path.exists(file_latencia):
        print(f"Error: file not found -> {file_latencia}")
        return [[] for _ in range(5)]

    # 1. Lê todas as linhas válidas primeiro
    with open(file_latencia, 'r') as fp:
        for line in fp:
            parts = line.strip().split()
            if len(parts) >= 2:
                todas_linhas.append(parts)

    # 2. Aplica o descarte (Slicing) na ordem temporal original
    # Se rps_fim for 0, o slice [:-0] falha, então tratamos isso
    linhas_validas = todas_linhas[rps_inicio : -rps_fim] if rps_fim > 0 else todas_linhas[rps_inicio:]

    print(f"File: {os.path.basename(file_latencia)}")
    print(f"  - Total: {len(todas_linhas)} | Discarded from start: {rps_inicio} | Discarded from end: {rps_fim}")

    # 3. Distribui nas categorias CH-1 a CH-5
    categorias = [[] for _ in range(5)]
    mapeamento = {
        "GET_INDEX": 0, "POST_CURRENCY": 1, "GET_PRODUCT": 2,
        "GET_CART": 3, "POST_CART": 4
    }

    for label, value in linhas_validas:
        if label in mapeamento:
            idx = mapeamento[label]
            categorias[idx].append(float(value) / 1000) # Conversão para ms

    return categorias

def draw_plot_comparativo(data_sig, data_sp):
    labels = ['CH-1', 'CH-2', 'CH-3', 'CH-4', 'CH-5']
    posicoes = np.array(range(len(labels))) + 1
    offset = 0.2

    plt.figure(figsize=(16, 9))
    max_percent = 99.9 
    flier = False
    
    #b1 = plt.boxplot(data_sig, vert=False, positions=posicoes + offset, widths=0.3, patch_artist=True, showfliers=False, boxprops=dict(facecolor="lightblue"))
    b1 = plt.boxplot(data_sig, vert=False, positions=posicoes + offset, widths=0.3, patch_artist=True, showfliers=flier, boxprops=dict(facecolor="lightblue"), whis=(0,max_percent))
    
    #b2 = plt.boxplot(data_sp, vert=False, positions=posicoes - offset, widths=0.3,  patch_artist=True, showfliers=False, boxprops=dict(facecolor="tab:red"))
    b2 = plt.boxplot(data_sp, vert=False, positions=posicoes - offset, widths=0.3,  patch_artist=True, showfliers=flier, boxprops=dict(facecolor="tab:red"),  whis=(0,max_percent))

    title = "Boxplot of each request at: " + str(max_percent)
    #plt.title(title, fontsize=25)
    plt.yticks(posicoes, labels, size=25)
    plt.xlabel('Latency (ms)', fontsize=25)
    plt.xticks(size=20)
    plt.legend([b1["boxes"][0], b2["boxes"][0]], ['SIGBPF', 'SPRIGHT'], loc='upper right', fontsize=25, edgecolor="black")
    
    plt.grid(axis='x', linestyle='--', alpha=0.6)
    plt.tight_layout()

    file_name = rps +'k_ch_boxplot.png'
    plt.savefig(file_name)
    #plt.show()

if __name__ == '__main__':

    if len(sys.argv) <= 1:
        print(f"Passe o numero de RPS: python3 {sys.argv[0]} RPS")
        exit(0)
        
    
    #path = "/mydata/sigbpf/trafficgen/"
    path_sig = "./sigbpf/lat/"
    path_sp = "./spright/lat/"

    RPS = sys.argv[1] 
    rps = RPS

    f_lat_sig = path_sig + "latencies_"     + RPS + "000rps-mod.txt"
    f_rps_sig = path_sig + "requests_recv_" + RPS + "000rps-mod.txt"
    
    f_lat_sp  = path_sp + "latencies_" +     RPS + "000rps-mod.txt"
    f_rps_sp  = path_sp + "requests_recv_" + RPS + "000rps-mod.txt"

    print("Processing data...")
    sig_data = coleta_dados_filtrados(f_lat_sig, f_rps_sig)
    sp_data  = coleta_dados_filtrados(f_lat_sp, f_rps_sp)

    draw_plot_comparativo(sig_data, sp_data)
    #draw_plot_sig(sig_data, sp_data)


