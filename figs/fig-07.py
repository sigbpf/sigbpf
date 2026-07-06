import matplotlib.pyplot as plt
import numpy as np
import matplotlib as mpl
import os
import sys
import matplotlib.ticker as ticker
from matplotlib.ticker import PercentFormatter

# Configurações de estilo
plt.rcParams['axes.labelsize'] = 15  
plt.rcParams['xtick.labelsize'] = 13
plt.rcParams['ytick.labelsize'] = 13
mpl.rcParams['pdf.fonttype'] = 42
plt.rcParams['font.sans-serif'] = ['Arial']
plt.rcParams["figure.figsize"] = (16, 9)

def obter_janela_valida(arquivo_rps):
    rps_por_segundo = []
    if not os.path.exists(arquivo_rps):
        print(f"Erro: Arquivo RPS nao encontrado -> {arquivo_rps}")
        return 0, 0
    with open(arquivo_rps, 'r') as f:
        for line in f:
            partes = line.split()
            if len(partes) >= 2:
                qtd_rps = int(partes[1])
                rps_por_segundo.append(qtd_rps)
    if len(rps_por_segundo) < 2:
        return 0, 0
    return rps_por_segundo[0], rps_por_segundo[-1]

def coleta_dados_sincronizados(file_latencia, arquivo_rps):
    rps_inicio, rps_fim = obter_janela_valida(arquivo_rps)
    todas_latencias = []
    if not os.path.exists(file_latencia):
        print(f"Erro: Arquivo de latencia nao encontrado -> {file_latencia}")
        return []
    with open(file_latencia, 'r') as f:
        for line in f:
            partes = line.split()
            if partes:
                try:
                    valor = float(partes[-1]) / 1000
                    todas_latencias.append(valor)
                except ValueError:
                    continue
    total_lido = len(todas_latencias)
    if total_lido > (rps_inicio + rps_fim):
        dados_filtrados = todas_latencias[rps_inicio : -rps_fim]
    else:
        dados_filtrados = []
    return dados_filtrados
    #return todas_latencias

# 1. Define a função de formatação para porcentagem
def to_percent(x, pos):
    # Multiplica por 100 para transformar em % (ex: 0.001 vira 0.1%)
    return f'{x*100:g}%'

###################################################################################
# FUNÇÃO ADAPTADA PARA CCDF LOG
###################################################################################
def plotar_ccdf_log(dados1, label1, dados2, label2, rps_alvo):
    if not dados1 or not dados2:
        print(f"Dados não encontrados para o RPS {rps_alvo}")
        return

    plt.figure()

    for dados, nome, cor in [(dados1, label1, 'tab:blue'), (dados2, label2, 'tab:red')]:
        # Ordena os dados
        dados_sorted = np.sort(dados)
        n = len(dados_sorted)
        
        # Calcula CCDF: P(X > x)
        # y_cdf = np.arange(1, n + 1) / n
        y_ccdf = 1 - (np.arange(1, n + 1) / n)
        
        # Removemos o último ponto para evitar log(0) no gráfico
        #plt.step(dados_sorted[:-1], y_ccdf[:-1], label=f"{nome}", color=cor, linewidth=5, where='post')
        plt.step(dados_sorted, y_ccdf, label=f"{nome}", color=cor, linewidth=5, where='post')

        # Cálculo do P99 apenas para informação no console ou legenda
        p99 = np.percentile(dados, 99.99)
        print(f"{nome} P99.99: {p99:.2f} ms")

    # Escala logarítmica no eixo Y (Probabilidade)
    plt.yscale('log')

    ax = plt.gca() # Pega o eixo atual
    ax.set_ylim(bottom=1e-6, top=1)
    ax.yaxis.set_major_formatter(ticker.ScalarFormatter())
    ax.yaxis.get_major_formatter().set_scientific(False)

    #ax.yaxis.set_major_formatter(PercentFormatter(xmax=1, decimals=3))
    ax.yaxis.set_major_formatter(ticker.FuncFormatter(to_percent))
    ax.yaxis.set_minor_formatter(ticker.NullFormatter())

    ax.yaxis.set_major_locator(ticker.LogLocator(base=10.0, numticks=10))
    ax.yaxis.set_minor_locator(ticker.LogLocator(base=10.0, subs='auto', numticks=10))
    
    #plt.title(f'Comparativo CCDF (Escala Log) - Carga: {rps_alvo} k', fontsize=35)
    plt.xlabel('Latency (ms)', fontsize=35)
    plt.ylabel('P(X > x)', fontsize=35)
    
    # Grade detalhada para escala logarítmica
    #plt.grid(True, which="both", linestyle='--', linewidth=1, alpha=0.7)
    plt.grid(True,  which="both", linestyle='--', linewidth=1, antialiased=True )

    plt.xticks(fontsize=35)
    plt.yticks(fontsize=35)
    
    plt.legend(loc='upper right', fontsize=20, frameon=True, edgecolor='black')

    plt.tight_layout()
   
    file_saved = f"{rps_alvo}k_ccdf_log.png"
    #if not os.path.exists("./figs/ccdf/"): os.makedirs("./figs/")
    plt.savefig(file_saved, pad_inches=0.01)
    #plt.show()

if __name__ == '__main__':

    if len(sys.argv) < 2:
        print("Uso: python script.py <RPS>")
        sys.exit(1)

    path_sig = "./sigbpf/lat/"
    path_sp = "./spright/lat/"
    RPS = sys.argv[1] 
    
    lat_sig = path_sig + "latencies_"     + RPS + "000rps-mod.txt"
    rps_sig = path_sig + "requests_recv_" + RPS + "000rps-mod.txt"
    
    lat_sp  = path_sp + "latencies_" +     RPS + "000rps-mod.txt"
    rps_sp  = path_sp + "requests_recv_" + RPS + "000rps-mod.txt"

    dados_sig = coleta_dados_sincronizados(lat_sig, rps_sig)
    dados_sp = coleta_dados_sincronizados(lat_sp, rps_sp)

    plotar_ccdf_log(dados_sig, "SIGBPF " + RPS + " k", dados_sp, "SPRIGHT " + RPS + " k", RPS)

