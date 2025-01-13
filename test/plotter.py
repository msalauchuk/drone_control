import sys
import matplotlib.pyplot as plt
import pandas as pd
import seaborn as sns

def plot_coverage_analysis(filename, output_filename="system.png"):

    # model timestep
    T = 2.4

    # Set the visual style for better-looking plots
    sns.set_style("whitegrid")

    # Read and process the data
    df = pd.read_csv(filename)

    # Convert time to minutes for better readability
    df['minutes'] = df['t'] * T/ 60

    # Create the visualization
    plt.figure(figsize=(12, 7))

    # Main coverage plot
    plt.plot(df['minutes'], df['verified_area'],
             linewidth=2,
             color='#2E86C1',
             label='Area Coverage')

    # Add key threshold lines
    plt.axhline(y=99, color='#27AE60', linestyle='--',
                label='Required Coverage (99%)')
    plt.axvline(x=15, color='#E74C3C', linestyle='--',
                label='Time Target (15 min)')

    # Enhance the plot appearance
    plt.title('Area Coverage Over Time',
              fontsize=14, pad=20)
    plt.ylabel('Verified Area (%)', fontsize=12)
    plt.xlabel('Time (minutes)', fontsize=12)

    # Add legend with better positioning
    plt.legend(loc='lower right', frameon=True)

    # Add grid for better readability
    plt.grid(True, which='both', linestyle='-', alpha=0.2)

    # Adjust layout and save
    plt.tight_layout()
    plt.savefig(output_filename, dpi=300, bbox_inches='tight')
    plt.close()

if __name__ == "__main__":
    if len(sys.argv) != 2:
        print("Usage: python plotter.py <filename.csv>")
        print("Example: python plotter.py system.csv")
        sys.exit(1)

    try:
        plot_coverage_analysis(sys.argv[1])
        print(f"Plot successfully saved as system.png")
    except Exception as e:
        print(f"Error: {str(e)}")
        sys.exit(1)