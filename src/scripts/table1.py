#!/usr/bin/python

import argparse
import pandas
import os

def main(results_dir):
    targets = [
        "openssl", "pidgin-otrng", "mbedtls", "netdata", "linux-fs",
        "linux-nfc", "linux-fullkernel", "littlefs", "zlib"
    ]
    spec_types = ["<0", "==0", "<=0", ">0", "!=0", ">=0"]

    # Target -> spec type -> count
    spec_counts = dict()

    for t in targets:
        spec_counts[t] = dict()
    for t in targets:
        for st in spec_types:
            spec_counts[t][st] = 0

    for t in targets:
        specs_file = os.path.join(results_dir, t + "-specs.txt")
        try:
          f = open(specs_file, 'r')
        except:
          continue
        lines = [x.strip() for x in f.readlines()]

        for l in lines:
            for st in spec_types:
                if st in l:
                    spec_counts[t][st] += 1
        f.close()

    df = pandas.DataFrame(columns=['Program'] + spec_types)

    for t in targets:
        df = df.append({
            "Program": t,
            "<0": spec_counts[t]["<0"],
            "==0": spec_counts[t]["==0"],
            "<=0": spec_counts[t]["<=0"],
            ">0": spec_counts[t][">0"],
            "!=0": spec_counts[t]["!=0"],
            ">=0": spec_counts[t][">=0"]
        }, ignore_index=True)

    print(df)


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument('--results', help="Path to results directory", required=True)
    args = parser.parse_args()

    main(args.results)
