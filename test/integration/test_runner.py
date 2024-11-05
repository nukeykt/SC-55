import subprocess
import argparse
import hashlib
import sys

parser = argparse.ArgumentParser(
    epilog="Arguments after the first '--' will be forwarded to the render executable."
)
parser.add_argument("--render-exe", type=str, required=True)
parser.add_argument("--sha256", type=str, required=True)


def main():
    try:
        dashdash = sys.argv.index("--")
        runner_args = sys.argv[1:dashdash]
        extra_args = sys.argv[dashdash + 1 :]
    except ValueError:
        runner_args = sys.argv[1:]
        extra_args = []

    args = parser.parse_args(runner_args)

    cmd = [
        args.render_exe,
        "--stdout",
    ] + extra_args

    with subprocess.Popen(cmd, stdout=subprocess.PIPE) as proc:
        digest = hashlib.file_digest(proc.stdout, "sha256")

    if digest.hexdigest().casefold() != args.sha256.casefold():
        print("hash mismatch")
        sys.exit(1)

    sys.exit(proc.wait())


if __name__ == "__main__":
    main()
