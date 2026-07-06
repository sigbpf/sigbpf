import re
import os
import matplotlib.pyplot as plt
import numpy as np

def extract_rps_value(text):
    match = re.search(r'(\d+)([kK]?)', text)
    if match:
        value = int(match.group(1))
        unit = match.group(2).lower()
        return value * 1000 if unit == 'k' else value
    return 0

def format_x_axis_label(text):
    match = re.search(r'(\d+)([kK]?)', text)
    if match:
        value = match.group(1)
        num = int(value) // 1000 if int(value) >= 1000 else value
        return f"{num} k"
    return text

def get_rps_discards(rps_path):
    if not os.path.exists(rps_path): return 0, 0
    rps_values = []
    #rps_10sec = []
    rps_10sec = 0 
    with open(rps_path, 'r') as f:
        for line in f:
            parts = line.split()
            if len(parts) >= 2:
                rps_values.append(int(parts[1]))
    if not rps_values: return 0, 0
    #return rps_values[0], rps_values[-1]
    for i in range(0, 10):
        rps_10sec = rps_10sec + rps_values[i] 

    return rps_10sec, rps_values[-1]

def collect_synchronized_data(lat_path, rps_path):
    rps_start, rps_end = get_rps_discards(rps_path)
    latencies = []
    if not os.path.exists(lat_path): return 0, 0, []
    with open(lat_path, 'r') as fp:
        for line in fp:
            parts = line.strip().split()
            if len(parts) >= 2:
                try:
                    latencies.append(float(parts[1]) / 1000)
                except ValueError: continue
    
    res = latencies[rps_start : -rps_end] if rps_end > 0 else latencies[rps_start:]
    return rps_start, rps_end, res

def load_ch_data(root_dir, ch_name):
    data = {}
    ch_path = os.path.join(root_dir, ch_name)
    if not os.path.exists(ch_path):
        print(f"[ERROR] Path not found: {ch_path}")
        return data
    
    print(f"\n>>> Reading data from: {ch_path}")
    folders = [d for d in os.listdir(ch_path) if os.path.isdir(os.path.join(ch_path, d))]
    folders.sort(key=extract_rps_value)
    
    for folder in folders:
        #print(f"Entering: {folder}...")
        folder_path = os.path.join(ch_path, folder)
        print(folder_path)
        #folder_path = folder_path + "/1-iteration"
        print(folder_path)
        all_latencies = []
        files = os.listdir(folder_path)
        lat_files = [f for f in files if f.startswith("latencies_") and f.endswith("-mod.txt")]
        
        for f_lat in lat_files:
            num_match = re.search(r'\d+', f_lat)
            if not num_match: continue
            target_num = num_match.group()
            f_rps = next((f for f in files if "requests_recv" in f and target_num in f), None)
            
            if f_rps:
                start, end, res = collect_synchronized_data(os.path.join(folder_path, f_lat), 
                                                           os.path.join(folder_path, f_rps))
                all_latencies.extend(res)
                print(f"  [OK] {ch_name}/{folder}: {f_lat} <-> {f_rps} | discarded: start={start} end={end}")
            else:
                print(f"  [WARNING] RPS not found to {f_lat} in {folder}")
        
        if all_latencies:
            data[folder] = all_latencies
    return data

def plot_multi_ch_comparison(ch_list, path1, path2, name1="SPRIGHT", name2="SIGBPF", rps_max=11000):
    all_data_1 = {ch: load_ch_data(path1, ch) for ch in ch_list}
    all_data_2 = {ch: load_ch_data(path2, ch) for ch in ch_list}

    # Intersection of common RPS across all CHs and architectures
    sets = []
    for ch in ch_list:
        if all_data_1[ch] and all_data_2[ch]:
            sets.append(set(all_data_1[ch].keys()) & set(all_data_2[ch].keys()))
    
    if not sets:
        print("No compatible data found for plotting.")
        return

    common_rps = sorted(list(set.intersection(*sets)), key=extract_rps_value)
    if rps_max:
        common_rps = [r for r in common_rps if extract_rps_value(r) <= rps_max]

    percentile = 99
    plt.figure(figsize=(16, 9))
    
    indices = np.arange(len(common_rps))
    n_chs = len(ch_list)
    group_width = 0.8
    bar_width = group_width / (n_chs * 2)

    # Color palettes
    colors_1 = ['#4b0000', '#8b0000', '#d00000', '#ff4d4d', '#ff9999'] # SPRIGHT
    colors_2 = ['#084594', '#2171b5', '#4292c6', '#6baed6', '#9ecae1'] # SIGBPF
    chain_label = ["L-02",  "L-04", "L-06", "L-08","L-16"] 

    for i, ch in enumerate(ch_list):
        #y1 = [np.percentile(all_data_1[ch][r], percentile) for r in common_rps]
        y2 = [np.percentile(all_data_2[ch][r], percentile) for r in common_rps]
        
        offset_1 = (i * 2) * bar_width - (group_width / 2)
        offset_2 = (i * 2 + 1) * bar_width - (group_width / 2)
        
        #plt.bar(indices + offset_2, y1, bar_width, label=f"{name1} {ch}", color=colors_1[i % 5])
        #plt.bar(indices + offset_2, y1, bar_width, label=f"{sigbpf_label[i % 5]}", color=colors_1[i % 5])
        plt.bar(indices + offset_1, y2, bar_width, label=f"{name2} {chain_label[i]}", color=colors_2[i % 5], hatch='//')
        #plt.bar(indices + offset_1, y2, bar_width, label=f"{spright_label[i % 5]}",color=colors_2[i % 5], hatch='//')

    for i, ch in enumerate(ch_list):
        y1 = [np.percentile(all_data_1[ch][r], percentile) for r in common_rps]
        #y2 = [np.percentile(all_data_2[ch][r], percentile) for r in common_rps]
        
        offset_1 = (i * 2) * bar_width - (group_width / 2)
        offset_2 = (i * 2 + 1) * bar_width - (group_width / 2)
        
        plt.bar(indices + offset_2, y1, bar_width, label=f"{name1} {chain_label[i]}", color=colors_1[i % 5])
        #plt.bar(indices + offset_2, y1, bar_width, label=f"{sigbpf_label[i % 5]}", color=colors_1[i % 5])
        #plt.bar(indices + offset_1, y2, bar_width, label=f"{name2} {ch}", color=colors_2[i % 5], hatch='//')
        #plt.bar(indices + offset_1, y2, bar_width, label=f"{spright_label[i % 5]}",color=colors_2[i % 5], hatch='//')

    #plt.title(f"Tail Latency Comparison (P{percentile}) - Multiple Chains", fontsize=22)
    plt.xticks(indices, [format_x_axis_label(r) for r in common_rps], fontsize=30)
    plt.yticks(fontsize=40)
    plt.ylim(0, 2.2)

    plt.ylabel('Latency (ms)', fontsize=30)
    plt.xlabel('Load (Req/s)', fontsize=30)

    plt.legend(fontsize=25, ncol=2)
    plt.grid(axis='y', linestyle='--', alpha=0.3)
    
    plt.tight_layout()
    #if not os.path.exists("./figs"): os.makedirs("./figs")
    file_path =  str(percentile) + "-multi_ch_tail_latency.png"
    plt.savefig(file_path)

    #print(f"\n[SUCCESS] Chart saved to ./figs/multi_ch_tail_latency.png")
    #plt.show()

if __name__ == "__main__":
    # Each dir  
    # sigbpf/chain/CH-1-5/RPS 1-10/latency
    # spright/chain/CH-1-5/RPS 1-10/latency
    chains = ['CH-1', 'CH-2', 'CH-3', 'CH-4', 'CH-5']
    plot_multi_ch_comparison(chains, './spright/chain/', './sigbpf/chain/', "SPRIGHT", "SIGBPF", rps_max=11000)

