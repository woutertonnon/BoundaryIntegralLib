import os
import glob
import subprocess
import pandas as pd
import matplotlib.pyplot as plt

# ==============================================================================
# Configuration
# ==============================================================================
EXECUTABLE = "../release/mgconvergence"       # Path to your compiled C++ executable
MESH_FOLDER = "meshes"        # Folder containing the mesh files
TEMP_CSV = "temp_result.csv"  # Temporary file for C++ output

# Dictionary mapping mesh filenames to specific refinement levels.
MESH_CONFIG = {
    "cube.msh": 4,
    "ball.msh": 4,
    "ball_hole.msh": 4,
    "corner.msh": 4,
    "corner_structured.msh": 4,
    "cylinder.msh": 4
}

GMRES_RUNS = 8                # Number of random RHS samples for GMRES stats
NEV = 2                       # Compute 2 eigenvalues to get the second one
EW_TOL = 1e-4                 # Tolerance for the eigenvalues (not GMRES)
GMRES_TOL = 1e-6

def run_study():
    # 1. Find all mesh files
    mesh_files = glob.glob(os.path.join(MESH_FOLDER, "*.msh"))
    mesh_files += glob.glob(os.path.join(MESH_FOLDER, "*.mesh"))

    if not mesh_files:
        print(f"Error: No mesh files found in folder '{MESH_FOLDER}'")
        return

    print(f"Found {len(mesh_files)} meshes in '{MESH_FOLDER}'.")

    # Structure to hold results: {'V': {mesh: df}, 'W': {mesh: df}}
    all_results = {'V': {}, 'W': {}}
    cycles = ['V', 'W']

    # 2. Iterate through cycles and meshes
    for cycle in cycles:
        print(f"\n========================================")
        print(f" Running {cycle}-Cycle Simulations")
        print(f"========================================")

        for mesh_path in mesh_files:
            mesh_filename = os.path.basename(mesh_path)
            mesh_name_no_ext = os.path.splitext(mesh_filename)[0]

            # Check if this mesh is in our config
            if mesh_filename not in MESH_CONFIG:
                # print(f"Skipping {mesh_filename} (not in MESH_CONFIG).")
                continue

            refinements = MESH_CONFIG[mesh_filename]
            print(f"Processing: {mesh_filename} (Cycle: {cycle}, Refs: {refinements})...")

            # Construct command line arguments
            cmd = [
                EXECUTABLE,
                "--mesh", mesh_path,
                "--refinements", str(refinements),
                "--output", TEMP_CSV,
                "--nev", str(NEV),
                "--gmres", str(GMRES_RUNS),
                "--eval_tol", str(EW_TOL),
                "--gmres_tol", str(GMRES_TOL),
                "--cycle", cycle
            ]

            try:
                # Run the C++ solver
                subprocess.run(cmd, check=True)

                # Read the results
                if os.path.exists(TEMP_CSV):
                    df = pd.read_csv(TEMP_CSV)
                    all_results[cycle][mesh_name_no_ext] = df
                else:
                    print(f"Warning: No output generated for {mesh_name_no_ext}")

            except subprocess.CalledProcessError as e:
                print(f"Error executing solver for {mesh_name_no_ext}. Return code: {e.returncode}")
            except Exception as e:
                print(f"An error occurred processing {mesh_name_no_ext}: {e}")

    # 3. Plotting
    # Check if we have any results
    if not any(all_results['V']) and not any(all_results['W']):
        print("\nNo results to plot.")
        return

    plot_results_2x2(all_results)

def plot_results_2x2(all_results):
    # Create a 2x2 grid
    # Row 0: V-Cycle, Row 1: W-Cycle
    # Col 0: Convergence, Col 1: GMRES
    fig, axes = plt.subplots(2, 2, figsize=(16, 12))

    colors = plt.cm.tab10.colors
    cycles = ['V', 'W']

    for row_idx, cycle in enumerate(cycles):
        results = all_results[cycle]
        ax_conv = axes[row_idx, 0]
        ax_gmres = axes[row_idx, 1]

        if not results:
            ax_conv.text(0.5, 0.5, f"No Data for {cycle}-Cycle", ha='center')
            ax_gmres.text(0.5, 0.5, f"No Data for {cycle}-Cycle", ha='center')
            continue

        for i, (name, df) in enumerate(results.items()):
            color = colors[i % len(colors)]

            # --- Plot Convergence Rate (Eigenvalue) ---
            if 'AbsEval1' in df.columns:
                df_clean = df.dropna(subset=['AbsEval1'])
                ax_conv.plot(df_clean['DOFs'], df_clean['AbsEval1'],
                             marker='o', linestyle='-', linewidth=2,
                             color=color, label=name)
            elif 'AbsEval0' in df.columns:
                print(f"Warning ({cycle}-cycle): 'AbsEval1' not found for {name}, using 'AbsEval0'")
                df_clean = df.dropna(subset=['AbsEval0'])
                ax_conv.plot(df_clean['DOFs'], df_clean['AbsEval0'],
                             marker='o', linestyle='--', linewidth=2,
                             color=color, label=name)

            # --- Plot GMRES Iterations ---
            if 'AvgGMRES' in df.columns:
                ax_gmres.plot(df['DOFs'], df['AvgGMRES'],
                              marker='s', linestyle='--', linewidth=2,
                              color=color, label=name)

        # Formatting Convergence Plot
        ax_conv.set_title(f"{cycle}-Cycle Convergence Factor", fontsize=14)
        ax_conv.set_xlabel("Degrees of Freedom (DOFs)", fontsize=12)
        ax_conv.set_ylabel(r"Largest Error Eigenvalue $|\lambda|_{max}$", fontsize=12)
        ax_conv.set_xscale('log')
        ax_conv.set_ylim(0, 1.1)
        ax_conv.grid(True, which="both", ls="-", alpha=0.4)
        ax_conv.legend(loc='best')

        # Formatting GMRES Plot
        ax_gmres.set_title(f"{cycle}-Cycle GMRES Iterations", fontsize=14)
        ax_gmres.set_xlabel("Degrees of Freedom (DOFs)", fontsize=12)
        ax_gmres.set_ylabel("Average Iterations", fontsize=12)
        ax_gmres.set_xscale('log')
        ax_gmres.grid(True, which="both", ls="-", alpha=0.4)
        ax_gmres.legend(loc='best')

    plt.tight_layout()
    output_img = "mesh_comparison_2x2.pdf"
    plt.savefig(output_img, dpi=300)
    print(f"\nPlots saved to {output_img}")
    plt.show()

if __name__ == "__main__":
    run_study()
