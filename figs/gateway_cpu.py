import matplotlib.pyplot as plt
import numpy as np
import matplotlib as mpl
import sys

mpl.rcParams['hatch.linewidth'] = 0.3

plt.rcParams['axes.labelsize'] = 15  # xy-axis label size
plt.rcParams['xtick.labelsize'] = 13  # x-axis ticks size
plt.rcParams['ytick.labelsize'] = 13  # y-axis ticks size
plt.rcParams['hatch.linewidth'] = 0.5
mpl.rcParams['pdf.fonttype'] = 42
mpl.rcParams['ps.fonttype'] = 42

percent_usr = []
percent_system = []
percent_total = []
labels = []

percent_usr_sp    = []
percent_system_sp = []
percent_total_sp  = []
labels_sp = []

def getData(file_name):
    arquivo = open(file_name, "r")
    lines = arquivo.readlines()
    flag = 0

    percent_usr = []
    percent_system = []
    percent_total = []
    labels = []

    for i in lines:
        if i.find("Average") != -1:
            if flag == 0:
                print(i)
                flag = 1
                continue
           
            aux = i.split() # quebra a linha em palavras
          
            # posicoes 3 e 4 sao as %usr %system e 7 %CPU total e 9 eh o comando
            print(f"{aux[0]} | {float(aux[3]):2.2f} | {float(aux[4]):2.2f} | {float(aux[7]):2.2f} | {aux[9]}")
            percent_usr.append(float(aux[3]).__round__(1))
            percent_system.append(float(aux[4]).__round__(1))
            percent_total.append(float(aux[7]).__round__(1))
            labels.append(aux[9])

    percent_usr = [float(item) for item in percent_usr ]
    percent_system = [float(item) for item in percent_system ]
    percent_total = [float(item) for item in percent_total]

    print(f"percent_usr: {percent_usr}")
    print(f"percent_system: {percent_system}")
    print(f"percent_total: {percent_total}")
    arquivo.close()

    return percent_usr, percent_system, percent_total, labels

###################################################################################

if len(sys.argv) <= 1:
    print(f"Provide the RPS: python3 {sys.argv[0]} <RPS>")
    exit(0)


path_sig = "sigbpf/cpu/"
path_sp = "spright/cpu/"
RPS = sys.argv[1] 

percent_usr, percent_system, percent_total, labels = getData(path_sig + RPS + "k-sigbpf_gw.cpu")
percent_usr_sp, percent_system_sp, percent_total_sp, labels_sp = getData(path_sp + RPS + "k-skmsg_gw.cpu")

cores_sigbpf = {'SIGBPF-USER': '#00a0e1', 'SIGBPF-SYSTEM': '#e6a532'}
cores_spright   = {'SPRIGHT-USER': '#41afaa', 'SPRIGHT-SYSTEM': '#d7642c'}


#cores_sigbpf = {'USER-SIGBPF': 'tab:blue', 'SYSTEM-SIGBPF': 'orange'}
#cores_sigbpf = {'SIGBPF-USER': 'tab:blue', 'SIGBPF-SYSTEM': 'orange'}
#cores_sigbpf = {'USER-SIGBPF': '#1f77b4', 'SYSTEM-SIGBPF': '#aec7e8'}
#cores_spright   = {'SPRIGHT-USER': 'tab:blue' , 'SPRIGHT-SYSTEM': 'orange'}
#cores_spright   = {'USER-SPRIGHT': '#ff7f0e' , 'SYSTEM-SPRIGHT': '#ffbb78'}

container = ["gateway"]

###################################################################################
# --- CONFIGURAÇÃO DO GRÁFICO ---
x = np.arange(len(labels))
width = 0.4  # Largura das barras
#width = 0.1  # Largura das barras

#fig, ax = plt.subplots(figsize=(12, 7))
#fig, ax = plt.subplots(figsize=(24, 12))
fig, ax = plt.subplots(figsize=(16, 9))

# 1. BARRA DA ESQUERDA (Empilhada: USR + SYSTEM)
#stack_data = {'USER': np.array(percent_usr), 'SYSTEM': np.array(percent_system)}
#stack_data = {'USER-SIGBPF': np.array(percent_usr), 'SYSTEM-SIGBPF': np.array(percent_system)}
stack_data = {'SIGBPF-USER': np.array(percent_usr), 'SIGBPF-SYSTEM': np.array(percent_system)}
bottom = np.zeros(len(labels))

#offset_left  = -width/2
offset_left  = -width/4
#offset_right = width/2
offset_right = width/4

for nome, valores in stack_data.items():
    #p_stack = ax.bar(x + offset_left, valores, width/4, label=nome, bottom=bottom,  edgecolor='black',  linewidth=1, color=cores_sigbpf[nome],  hatch='/')
    p_stack = ax.bar(x + offset_left, valores, width/4, label=nome, bottom=bottom,  edgecolor='black',  linewidth=1, color=cores_sigbpf[nome])
    # Adiciona os valores DENTRO dos segmentos da pilha
    #ax.bar_label(p_stack, padding=3, label_type='center', fontweight='bold', fontsize=25)
    #ax.bar_label(p_stack, padding=3, label_type='center', fontsize=25)
    bottom += valores

#p_total = ax.bar(x + offset_left, percent_total, width, label='Total CPU', color='none')
p_total = ax.bar(x + offset_left, percent_total, width, color='none')
# Adiciona o valor no TOPO da barra da direita
#ax.bar_label(p_total, padding=3, fontweight='bold', color='black', fontsize=33)
ax.bar_label(p_total, padding=3,  color='black', fontsize=33)

###################################################################################

# 2. BARRA DA DIREITA (Empilhada: USR + SYSTEM)
#stack_data2 = {'USER': np.array(percent_usr_sp), 'SYSTEM': np.array(percent_system_sp)}
#stack_data2 = {'USER-SPRIGHT': np.array(percent_usr_sp), 'SYSTEM-SPRIGHT': np.array(percent_system_sp)}
stack_data2 = {'SPRIGHT-USER': np.array(percent_usr_sp), 'SPRIGHT-SYSTEM': np.array(percent_system_sp)}
bottom = np.zeros(len(labels_sp))
for nome, valores in stack_data2.items():
    #p_stack2 = ax.bar(x + offset_right, valores, width/4, label=nome, bottom=bottom, edgecolor='black',  linewidth=1 ,color=cores[nome])
    p_stack2 = ax.bar(x + offset_right, valores, width/4, label=nome, bottom=bottom, edgecolor='black',  linewidth=1, color=cores_spright[nome])
    
    # Adiciona os valores DENTRO dos segmentos da pilha
    #ax.bar_label(p_stack2, padding=3, label_type='center', fontweight='bold', fontsize=35)
    #ax.bar_label(p_stack2, padding=3, label_type='center',  fontsize=25)
    bottom += valores

# 2. BARRA DA DIREITA (Simples: TOTAL CPU)
#p_total = ax.bar(x + offset_right, percent_total_sp, width, label='Total CPU', color='none')
p_total = ax.bar(x + offset_right, percent_total_sp, width,  color='none')

# Adiciona o valor no TOPO da barra da direita
ax.bar_label(p_total, padding=3,  color='black', fontsize=33)

###################################################################################
# --- ESTILIZAÇÃO E LEGENDA ---
ax.set_ylabel('CPU Usage (%)', size=30)
#ax.set_title('CPU usage: SIGBPF(Left stack) vs SPRIGHT(Right stack) at 11K RPS')
ax.set_xticks(x, columnspacing = 0.7, labelspacing = 0.1,fontsize=25)
#ax.set_xticklabels(labels, rotation=45, ha='right', fontweight='bold')
ax.set_xticklabels(container, rotation=45, ha='right',fontsize=30)

# Define o limite de 0 a 100
ax.set_ylim(0, 100)

# Define os saltos de 20 em 20
ax.set_yticks(np.arange(0, 101, 20), size=35, columnspacing = 0.7, labelspacing = 0.1)
plt.yticks(fontsize=40)

# Legenda interna
#ax.legend(loc='upper right', frameon=True, shadow=True)
#ax.legend(loc='upper right', frameon=True, edgecolor="black", fontsize=25)
ax.legend(loc='upper right', frameon=True, edgecolor="black", fontsize=30,   columnspacing = 0.7, labelspacing = 0.1)

plt.grid(True, linestyle='--')
plt.tight_layout()
#file_name = RPS + "rps_gateway_cpu.pdf"
file_name = RPS + "rps_gateway_cpu.png"
#plt.savefig('./figs/'+file_name, bbox_inches='tight',  dpi=fig.dpi,pad_inches=0.01)
plt.savefig(file_name, bbox_inches='tight',  dpi=fig.dpi,pad_inches=0.01)
#plt.savefig('gateway_cpu.pdf')
#plt.show()


