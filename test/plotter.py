import sys
import matplotlib.pyplot as plt
import pandas as pd

def plot_csv(filename):
    # Read the CSV file into a pandas DataFrame
    df = pd.read_csv(filename)

    # Extract data from DataFrame columns
    verified_area = df['verified_area']
    t = df['t']

    # Plot the data
    plt.figure(figsize=(8, 5))
    plt.plot( t,verified_area,  )
    plt.title('Verified Area vs. t')
    plt.ylabel('Verified Area')
    plt.xlabel('t')
    plt.grid(True,which='major',linestyle='-')
    plt.savefig("system.png")
if __name__ == "__main__":
    if len(sys.argv) != 2:
        print("Usage: python script_name.py <filename.csv>")
        sys.exit(1)

    filename = sys.argv[1]
    plot_csv(filename)
