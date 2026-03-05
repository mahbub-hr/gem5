import matplotlib.pyplot as plt
import numpy as np
import json, argparse, os

def generate_plot(input_file):
    # 1. Load Data
    with open(input_file, 'r') as f:
        data = json.load(f)

    benchmarks = list(data.keys())
    # Assuming all benchmarks have the same configs
    configs = list(data[benchmarks[0]].keys()) 
    outcomes = ["SDC", "CRASH", "DETECTED", "MASKED"]
    colors = ['#C00000', '#ED7D31', '#FFC000', '#A9D08E'] # Red, Orange, Yellow, Green

    # 2. Process Data for Plotting
    # We need a 3D structure: [Outcome][Benchmark][Config]
    plot_data = {outcome: [] for outcome in outcomes}
    
    for bench in benchmarks:
        for outcome in outcomes:
            # Get values for [duplicated, partitioned_duplicated]
            row = [data[bench][config].get(outcome, 0) for config in configs]
            plot_data[outcome].append(row)

    for outcome in outcomes:
        plot_data[outcome] = np.array(plot_data[outcome])

    # 3. Setup Plot
    x = np.arange(len(benchmarks))
    width = 0.35
    fig, ax = plt.subplots(figsize=(10, 6))

    # 4. Draw Bars
    for b_idx, config_label in enumerate(configs):
        offset = (b_idx - 0.5) * width if len(configs) > 1 else 0
        bottoms = np.zeros(len(benchmarks))
        
        for o_idx, outcome in enumerate(outcomes):
            values = plot_data[outcome][:, b_idx]
            # Normalize to 100%
            total_injections = np.sum([plot_data[o][:, b_idx] for o in outcomes], axis=0)
            percentages = (values / total_injections) * 100
            
            ax.bar(x + offset, percentages, width, bottom=bottoms, 
                   label=outcome if b_idx == 0 else "", color=colors[o_idx],
                   edgecolor='white', linewidth=0.5)
            bottoms += percentages

    # 5. Formatting
    ax.set_ylabel('Outcome Distribution (%)')
    ax.set_title('Fault Injection Results by Benchmark and Configuration')
    ax.set_xticks(x)
    ax.set_xticklabels(benchmarks)
    ax.legend(loc='upper center', bbox_to_anchor=(0.5, 1.15), ncol=4)
    
    # Label Configs under the bars
    for i in x:
        ax.text(i - width/2, -5, 'Dup', ha='center', fontsize=8, rotation=45)
        ax.text(i + width/2, -5, 'Part', ha='center', fontsize=8, rotation=45)

    plt.tight_layout()
    plt.savefig(os.path.join(os.path.dirname(input_file), "fault_results.png"), dpi=300)
    print("Plot saved as fault_results.png")

if __name__ == "__main__":
    parser= argparse.ArgumentParser(description="Plot Data")
    parser.add_argument(
        "--file",
        type=str,
        help="Path to the statistics file"
    )

    args = parser.parse_args()
    generate_plot(args.file)