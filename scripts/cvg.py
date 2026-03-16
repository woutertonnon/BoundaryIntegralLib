import os
import glob
import subprocess
import csv
import matplotlib.pyplot as plt
import argparse
import numpy as np
import itertools
from concurrent.futures import ThreadPoolExecutor, as_completed

# ==============================================================================
# Configuration
# ==============================================================================
EXECUTABLE = "../release/mgconvergence"
MESH_FOLDER = "../meshes"
OUTPUT_FOLDER = "out"
PLOT_FOLDER = "plots"

NUM_JOBS = 6  # Number of parallel jobs

# Default number of refinements for all meshes
DEFAULT_REFINEMENTS = 4

# Penalty Configuration
PENALTY_VALUES = [10.0, 20.0, 30.0, 40.0]

# Time-stepping (Tau) Configuration
TAU_VALUES = [0.0, 10.0, 100.0, 1000.0]

# Mesh configuration: Using the default for all
MESH_CONFIG = {
    "cube.msh": DEFAULT_REFINEMENTS,
    "ball.msh": DEFAULT_REFINEMENTS,
    "ball_hole.msh": DEFAULT_REFINEMENTS,
    "corner.msh": DEFAULT_REFINEMENTS,
    "corner_structured.msh": DEFAULT_REFINEMENTS,
    "cylinder.msh": DEFAULT_REFINEMENTS
}

GMRES_RUNS = 8
NEV = 2
EW_TOL = 1e-3
GMRES_TOL = 1e-6

def parse_arguments():
    parser = argparse.ArgumentParser(description="Run StokesMG penalty and tau sensitivity study.")
    parser.add_argument('--rerun', action='store_true', help="Force re-run of all simulations.")
    parser.add_argument('--plot-only', action='store_true', help="Only plot existing data without running.")
    return parser.parse_args()

def purge_old_files():
    """Deletes existing CSVs in the output folder and PDFs in the plots folder."""
    print("Automatically purging old files...")
    
    csv_files = glob.glob(os.path.join(OUTPUT_FOLDER, "*.csv"))
    for f in csv_files:
        try: os.remove(f)
        except OSError as e: print(f"Error deleting {f}: {e}")
            
    pdf_files = glob.glob(os.path.join(PLOT_FOLDER, "*.pdf"))
    for f in pdf_files:
        try: os.remove(f)
        except OSError as e: print(f"Error deleting {f}: {e}")
            
    print(f"Purged {len(csv_files)} CSV files and {len(pdf_files)} PDF plots.\n")

def run_job(cmd, out_file, rerun):
    if not rerun and os.path.exists(out_file):
        print(f"Skipping {os.path.basename(out_file)}, already exists.")
        return True
    
    print(f"Running: {' '.join(cmd)}")
    try:
        subprocess.run(cmd, check=True, stdout=subprocess.DEVNULL, stderr=subprocess.STDOUT)
        return True
    except subprocess.CalledProcessError as e:
        print(f"Job failed: {' '.join(cmd)}\n{e}")
        return False

def read_csv(filepath):
    data = {}
    with open(filepath, 'r') as f:
        reader = csv.DictReader(f)
        for row in reader:
            for key, val in row.items():
                if key not in data: data[key] = []
                data[key].append(float(val) if val != 'NaN' else np.nan)
    for key in data:
        data[key] = np.array(data[key])
    return data

def main():
    args = parse_arguments()

    os.makedirs(OUTPUT_FOLDER, exist_ok=True)
    os.makedirs(PLOT_FOLDER, exist_ok=True)

    if not args.plot_only:
        purge_old_files()

    jobs = []

    # 1. Setup Jobs
    for tau in TAU_VALUES:
        for p in PENALTY_VALUES:
            for cycle in ['V']:
                for mesh_file, refs in MESH_CONFIG.items():
                    mesh_path = os.path.join(MESH_FOLDER, mesh_file)
                    mesh_name = os.path.basename(mesh_file)
                    
                    if not os.path.exists(mesh_path): continue
                    
                    p_val = round(p, 2)
                    out_file = os.path.join(OUTPUT_FOLDER, f"out_{mesh_name}_tau_{tau}_p_{p_val}_{cycle}.csv")
                    cmd = [
                        EXECUTABLE,
                        "--mesh", mesh_path,
                        "--refinements", str(refs),
                        "--output", out_file,
                        "--tau", str(tau),
                        "--penalty", str(p_val),
                        "--cycle", cycle,
                        "--nev", str(NEV),
                        "--gmres", str(GMRES_RUNS),
                        "--gmres_tol", str(GMRES_TOL),
                        "--eval_tol", str(EW_TOL)
                    ]
                    jobs.append((cmd, out_file))

    # 2. Execute Jobs
    if not args.plot_only:
        with ThreadPoolExecutor(max_workers=NUM_JOBS) as executor:
            futures = [executor.submit(run_job, cmd, out_file, True) for cmd, out_file in jobs]
            for future in as_completed(futures): future.result()

    # 3. Read Results
    all_results = {t: {round(p, 2): {'V': {}, 'W': {}} for p in PENALTY_VALUES} for t in TAU_VALUES}
    
    for tau in TAU_VALUES:
        for p in PENALTY_VALUES:
            p_val = round(p, 2)
            for cycle in ['V', 'W']:
                for mesh_file in MESH_CONFIG.keys():
                    mesh_name = os.path.basename(mesh_file)
                    out_file = os.path.join(OUTPUT_FOLDER, f"out_{mesh_name}_tau_{tau}_p_{p_val}_{cycle}.csv")
                    if os.path.exists(out_file):
                        all_results[tau][p_val][cycle][mesh_name] = read_csv(out_file)

    # Calculate extremes
    tau_ext = list(dict.fromkeys([TAU_VALUES[0], TAU_VALUES[-1]]))
    pen_ext = list(dict.fromkeys([PENALTY_VALUES[0], PENALTY_VALUES[-1]]))
    extremes_combos = list(itertools.product(tau_ext, pen_ext))

    # 4. Plot Results
    for mesh_file in MESH_CONFIG.keys():
        mesh_name = os.path.basename(mesh_file)
        max_ref = MESH_CONFIG[mesh_file]
        
        for cycle in ['V', 'W']:
            has_data = any(mesh_name in all_results[t][round(p, 2)][cycle] for t in TAU_VALUES for p in PENALTY_VALUES)
            if not has_data: continue

            # ==================================================================
            # PLOT A: 2x2 Summary Plot
            # ==================================================================
            fig, axes = plt.subplots(2, 2, figsize=(15, 11))
            fig.suptitle(rf"Mesh: {mesh_name} | Cycle: {cycle}", fontsize=18)

            ax_ref_cvg = axes[0, 0]
            ax_ref_gmres = axes[0, 1]
            ax_pen_sens = axes[1, 0]
            ax_tau_sens = axes[1, 1]
            
            # Twin axes for sensitivity plots
            ax_pen_sens_gmres = ax_pen_sens.twinx()
            ax_tau_sens_gmres = ax_tau_sens.twinx()

            colors_top = plt.cm.tab10(np.linspace(0, 1, max(1, len(extremes_combos))))
            colors_bot = plt.cm.Set1(np.linspace(0, 1, max(1, max(len(tau_ext), len(pen_ext)))))

            # --- Top-Left & Top-Right: Refinements vs CVG and GMRES (Extreme Combos) ---
            for idx, (t, p) in enumerate(extremes_combos):
                p_val = round(p, 2)
                refs_plot, cvg_plot, gmres_plot = [], [], []
                if mesh_name in all_results[t][p_val][cycle]:
                    df = all_results[t][p_val][cycle][mesh_name]
                    for ref in range(1, max_ref + 1):
                        mask = df['Refinements'] == ref
                        if np.any(mask):
                            refs_plot.append(ref)
                            target_col = 'AbsEval1' if 'AbsEval1' in df else 'AbsEval0'
                            cvg_plot.append(df[target_col][mask][0])
                            gmres_plot.append(df['AvgGMRES'][mask][0])

                if refs_plot:
                    label = rf"$\tau$={t}, $C_w$={p_val}"
                    ls = '-' if idx < len(extremes_combos)/2 else '--'
                    ax_ref_cvg.plot(refs_plot, cvg_plot, marker='o', color=colors_top[idx], linestyle=ls, label=label)
                    ax_ref_gmres.plot(refs_plot, gmres_plot, marker='s', color=colors_top[idx], linestyle=ls, label=label)

            ax_ref_cvg.set_title("Convergence vs Refinements (Parameter Extremes)")
            ax_ref_cvg.set_xlabel("Number of Refinements")
            ax_ref_cvg.set_ylabel("Max Eigenvalue")
            ax_ref_cvg.set_xticks(range(1, max_ref + 1))
            ax_ref_cvg.grid(True, alpha=0.4)
            if ax_ref_cvg.has_data(): ax_ref_cvg.legend(loc='best')

            ax_ref_gmres.set_title("GMRES Iterations vs Refinements (Parameter Extremes)")
            ax_ref_gmres.set_xlabel("Number of Refinements")
            ax_ref_gmres.set_ylabel("Avg GMRES Iterations")
            ax_ref_gmres.set_xticks(range(1, max_ref + 1))
            ax_ref_gmres.grid(True, alpha=0.4)
            if ax_ref_gmres.has_data(): ax_ref_gmres.legend(loc='best')

            # --- Bottom-Left: C_w Sensitivity (Extreme Taus, At Max Refinement) with Twin Axis ---
            for idx, t in enumerate(tau_ext):
                p_plot, c_plot, g_plot = [], [], []
                for p in PENALTY_VALUES:
                    p_val = round(p, 2)
                    if mesh_name in all_results[t][p_val][cycle]:
                        df = all_results[t][p_val][cycle][mesh_name]
                        mask = df['Refinements'] == max_ref
                        if np.any(mask):
                            p_plot.append(p_val)
                            target_col = 'AbsEval1' if 'AbsEval1' in df else 'AbsEval0'
                            c_plot.append(df[target_col][mask][0])
                            g_plot.append(df['AvgGMRES'][mask][0])
                if p_plot:
                    ax_pen_sens.plot(p_plot, c_plot, marker='o', linestyle='-', color=colors_bot[idx], label=rf"$\tau$={t} (Eig)")
                    ax_pen_sens_gmres.plot(p_plot, g_plot, marker='x', linestyle='--', color=colors_bot[idx], label=rf"$\tau$={t} (GMRES)")

            ax_pen_sens.set_title(rf"$C_w$ Sensitivity (at ref = {max_ref})")
            ax_pen_sens.set_xlabel(rf"$C_w$")
            ax_pen_sens.set_ylabel("Max Eigenvalue")
            ax_pen_sens_gmres.set_ylabel("Avg GMRES Iterations")
            ax_pen_sens.grid(True, alpha=0.4)
            
            # Combine legends for Bottom-Left
            lines_1, labels_1 = ax_pen_sens.get_legend_handles_labels()
            lines_2, labels_2 = ax_pen_sens_gmres.get_legend_handles_labels()
            if lines_1 or lines_2:
                ax_pen_sens.legend(lines_1 + lines_2, labels_1 + labels_2, loc='best', fontsize=9)

            # --- Bottom-Right: Tau Sensitivity (Extreme Penalties, At Max Refinement) with Twin Axis ---
            for idx, p in enumerate(pen_ext):
                p_val = round(p, 2)
                t_plot, c_plot, g_plot = [], [], []
                for t in TAU_VALUES:
                    if mesh_name in all_results[t][p_val][cycle]:
                        df = all_results[t][p_val][cycle][mesh_name]
                        mask = df['Refinements'] == max_ref
                        if np.any(mask):
                            t_plot.append(t)
                            target_col = 'AbsEval1' if 'AbsEval1' in df else 'AbsEval0'
                            c_plot.append(df[target_col][mask][0])
                            g_plot.append(df['AvgGMRES'][mask][0])
                if t_plot:
                    t_strs = [str(x) for x in t_plot]
                    ax_tau_sens.plot(t_strs, c_plot, marker='o', linestyle='-', color=colors_bot[idx], label=rf"$C_w$={p_val} (Eig)")
                    ax_tau_sens_gmres.plot(t_strs, g_plot, marker='x', linestyle='--', color=colors_bot[idx], label=rf"$C_w$={p_val} (GMRES)")

            ax_tau_sens.set_title(rf"$\tau$ Sensitivity (at ref = {max_ref})")
            ax_tau_sens.set_xlabel(rf"$\tau$")
            ax_tau_sens.set_ylabel("Max Eigenvalue")
            ax_tau_sens_gmres.set_ylabel("Avg GMRES Iterations")
            ax_tau_sens.grid(True, alpha=0.4)
            
            # Combine legends for Bottom-Right
            lines_3, labels_3 = ax_tau_sens.get_legend_handles_labels()
            lines_4, labels_4 = ax_tau_sens_gmres.get_legend_handles_labels()
            if lines_3 or lines_4:
                ax_tau_sens.legend(lines_3 + lines_4, labels_3 + labels_4, loc='best', fontsize=9)

            plt.tight_layout()
            plt.subplots_adjust(top=0.92)
            summary_plot_file = os.path.join(PLOT_FOLDER, f"{mesh_name}_{cycle}_summary.pdf")
            plt.savefig(summary_plot_file, bbox_inches='tight')
            plt.close(fig)

            # ==================================================================
            # PLOT B: Heatmaps showing dependence on BOTH Tau and Penalty
            # ==================================================================
            fig_hm, axes_hm = plt.subplots(1, 2, figsize=(16, 7))
            fig_hm.suptitle(rf"Convergence Heatmaps (Mesh: {mesh_name} | {cycle}-Cycle)" + "\n" + f"At Maximum Refinement ({max_ref})", fontsize=16)

            Z_eig = np.full((len(PENALTY_VALUES), len(TAU_VALUES)), np.nan)
            Z_gmres = np.full((len(PENALTY_VALUES), len(TAU_VALUES)), np.nan)

            for i_p, p in enumerate(PENALTY_VALUES):
                p_val = round(p, 2)
                for i_t, t in enumerate(TAU_VALUES):
                    if mesh_name in all_results[t][p_val][cycle]:
                        df = all_results[t][p_val][cycle][mesh_name]
                        mask = df['Refinements'] == max_ref
                        if np.any(mask):
                            target_col = 'AbsEval1' if 'AbsEval1' in df else 'AbsEval0'
                            Z_eig[i_p, i_t] = df[target_col][mask][0]
                            Z_gmres[i_p, i_t] = df['AvgGMRES'][mask][0]

            def plot_single_heatmap(ax, Z_data, title, val_fmt):
                if np.all(np.isnan(Z_data)):
                    ax.set_visible(False)
                    return

                cmap = 'viridis_r'
                cax = ax.imshow(Z_data, origin='lower', aspect='auto', cmap=cmap)
                fig_hm.colorbar(cax, ax=ax, label=title)

                ax.set_xticks(np.arange(len(TAU_VALUES)))
                ax.set_xticklabels([str(t) for t in TAU_VALUES])
                ax.set_yticks(np.arange(len(PENALTY_VALUES)))
                ax.set_yticklabels([str(p) for p in PENALTY_VALUES])

                ax.set_xlabel(rf"$\tau$", fontsize=12)
                ax.set_ylabel(rf"$C_w$", fontsize=12)
                ax.set_title(title, fontsize=14)

                vmin = np.nanmin(Z_data)
                vmax = np.nanmax(Z_data)

                for i_p in range(len(PENALTY_VALUES)):
                    for i_t in range(len(TAU_VALUES)):
                        val = Z_data[i_p, i_t]
                        if not np.isnan(val):
                            norm_val = (val - vmin) / (vmax - vmin) if vmax > vmin else 0.5
                            text_col = "white" if norm_val > 0.5 else "black"
                            fmt_str = f"{{:{val_fmt}}}"
                            ax.text(i_t, i_p, fmt_str.format(val), ha="center", va="center", 
                                    color=text_col, fontweight='bold', fontsize=11)

            plot_single_heatmap(axes_hm[0], Z_eig, 'Max Eigenvalue (Convergence Rate)', '.3f')
            plot_single_heatmap(axes_hm[1], Z_gmres, 'Avg GMRES Iterations', '.1f')

            plt.tight_layout()
            plt.subplots_adjust(top=0.88)
            heatmap_plot_file = os.path.join(PLOT_FOLDER, f"{mesh_name}_{cycle}_heatmap.pdf")
            plt.savefig(heatmap_plot_file, bbox_inches='tight')
            plt.close(fig_hm)

    print(f"All done! PDF plots are saved in the '{PLOT_FOLDER}' directory.")

if __name__ == "__main__":
    main()