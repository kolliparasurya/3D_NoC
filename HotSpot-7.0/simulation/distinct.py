# filename: check_last_numbers.py

def check_lines(filename):
    with open(filename, 'r') as f:
        for line_number, line in enumerate(f, start=1):
            parts = line.strip().split()
            if len(parts) < 2:
                continue  # skip lines that don't have enough numbers
            try:
                last_num = float(parts[-1])
                second_last = float(parts[-2])
                if last_num < second_last:
                    print(f"Line {line_number}: {line.strip()}")
            except ValueError:
                print(f"Skipping line {line_number} (non-numeric values): {line.strip()}")

# Example usage
if __name__ == "__main__":
    filename = "data.txt"  # replace with your file name
    check_lines(filename)

