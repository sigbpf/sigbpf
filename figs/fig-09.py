import matplotlib.pyplot as plt
import numpy as np
import matplotlib as mpl
import sys

mpl.rcParams['hatch.linewidth'] = 0.3

plt.rcParams['axes.labelsize'] = 15 
plt.rcParams['xtick.labelsize'] = 35  
plt.rcParams['ytick.labelsize'] = 20  
plt.rcParams['hatch.linewidth'] = 0.5
mpl.rcParams['pdf.fonttype'] = 42
mpl.rcParams['ps.fonttype'] = 42

plt.rcParams['font.sans-serif'] = ['Arial']
plt.rcParams["figure.figsize"] = (16, 9)

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
           
            aux = i.split() 
          
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


if len(sys.argv) <= 1:
    print(f"Provide the RPS: python3 {sys.argv[0]} <RPS>")
    exit(0)

path_sig = "sigbpf/cpu/"
path_sp = "spright/cpu/"
RPS = sys.argv[1] 

percent_usr, percent_system, percent_total, labels = getData(path_sig + RPS + "k-sigbpf_fn.cpu")
percent_usr_sp, percent_system_sp, percent_total_sp, labels_sp = getData(path_sp + RPS + "k-skmsg_fn.cpu")

cores_sigbpf = {'SIGBPF-USER': '#00a0e1', 'SIGBPF-SYSTEM': '#e6a532'}
cores_spright   = {'SPRIGHT-USER': '#41afaa', 'SPRIGHT-SYSTEM': '#d7642c'}
containers = ["frontend", "currency","productcatalog", "cart", "recommendation", "shipping", "ad"]

###################################################################################
x = np.arange(len(labels))
width = 0.35 

fig, ax = plt.subplots()

stack_data = {'SIGBPF-USER': np.array(percent_usr), 'SIGBPF-SYSTEM': np.array(percent_system)}
bottom = np.zeros(len(labels))

offset_left  = -0.22
offset_right = 0.22

for nome, valores in stack_data.items():
    p_stack = ax.bar(x + offset_left, valores, width, label=nome, bottom=bottom,  edgecolor='black',  linewidth=1, color=cores_sigbpf[nome])
    bottom += valores

p_total = ax.bar(x + offset_left, percent_total, width,  color='none')
ax.bar_label(p_total, padding=3,  color='black', fontsize=33)

###################################################################################

stack_data2 = {'SPRIGHT-USER': np.array(percent_usr_sp), 'SPRIGHT-SYSTEM': np.array(percent_system_sp)}
bottom = np.zeros(len(labels_sp))

for nome, valores in stack_data2.items():
    p_stack2 = ax.bar(x + offset_right, valores, width, label=nome, bottom=bottom, edgecolor='black',  linewidth=1 ,color=cores_spright[nome])
    bottom += valores

p_total = ax.bar(x + offset_right, percent_total_sp, width,  color='none')
ax.bar_label(p_total, padding=3, color='black', fontsize=33)

ax.set_ylabel('CPU Usage (%)', fontsize=35)
ax.set_xticks(x,columnspacing = 0.7, labelspacing = 0.1)

print(f"labels:{labels}")
print(f"Containers:{containers}")

ax.set_xticklabels(containers, rotation=45, ha='right', fontsize=30)
ax.set_ylim(0, 100)
ax.set_yticks(np.arange(0, 101, 10))

plt.yticks(fontsize=40 )
plt.xticks(fontsize=30 )
plt.grid(True, linestyle='--')

ax.legend(loc='upper right', frameon=True, edgecolor="black", fontsize=30, columnspacing = 0.7, labelspacing = 0.1 )

plt.tight_layout()
#file_name = RPS + "rps_fn_cpu.pdf"
file_name = RPS + "rps_fn_cpu.png"
#plt.savefig('./figs/'+file_name,  bbox_inches='tight',dpi=400, pad_inches=0.01)
plt.savefig(file_name,  bbox_inches='tight',dpi=400, pad_inches=0.01)
#plt.show()

