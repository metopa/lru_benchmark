import csv
from pathlib import Path
import subprocess

def load_data(data_file):
    data = []
    with open(data_file) as tsvfile:
        reader = csv.reader(tsvfile, delimiter='\t')
        for i, row in enumerate(reader):
            if i > 0:
                data.extend(float(x) for x in row[1:])
    return data

def premultiply_hitrate_file(data_file):
    data = []
    with open(data_file) as tsvfile:
        reader = csv.reader(tsvfile, delimiter='\t')
        for i, row in enumerate(reader):
            if i > 0:
                row = row[:1] + [str(float(x) * 100) for x in row[1:]]
            data.append(row)
    with open(data_file, "w") as tsvfile:
        csv.writer(tsvfile, delimiter='\t').writerows(data)
        

def premultiply_hitrate(folder):
    folder = Path(folder)

    for f in folder.glob('*p.tsv'):
        premultiply_hitrate_file(str(f))


def get_cmd(script_file, data_file, cbcount=5):
    data = load_data(data_file)
    min_value, max_value = min(data), max(data)
    step = (max_value - min_value) / (cbcount - 1)
    max_value += step / 1000

    return ['gnuplot', '-e', f'input_file=\'{data_file}\'', '-e', f'output_file=\'{data_file.replace(".tsv", ".eps")}\'',
            '-e', f'data_min=\'{min_value:.5f}\'', '-e', f'data_step=\'{step:.6f}\'','-e', f'data_max=\'{max_value:.5f}\'', script_file]

def process_folder(folder):
    folder = Path(folder)

    for f in folder.glob('*.tsv'):
        script = 'meta-hitrate.gnuplot' if 'p.tsv' in str(f) else 'meta.gnuplot'
        cmd = get_cmd(script, str(f))
        #print(' '.join(cmd))
        subprocess.run(cmd)

if __name__ == "__main__":
    process_folder(".")
    # premultiply_hitrate(".")
