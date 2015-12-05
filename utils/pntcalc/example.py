import csv
import pntcalc

## Brief testing file to illustrate how the _pntcalc module can be used.

def read_csv(num_rows):
    data = []
    with open('../../GCDC102_20121016_074954.csv', 'r') as f:
        reader = csv.reader(f)
        count = 0
        for row in reader:
            data += row[-3:]
            count += 1
            if (count == num_rows):
                break
    return data


def main():
    num_rows = 24694 # total number of rows to be processed
    data = read_csv(
        num_rows) # data in a 1D list. Each row is a group of three consecutive elements in the list. Meaning the list should be num_rows*3 elements long
    device_milli = 0 # The device milliseconds
    day_milli = 10000000; # The day milliseconds
    start_row = 0 # The row in data at which to start. Note that each row has 3 values in it (one for each direction).
    vals = pntcalc.calculate(data, num_rows, start_row, device_milli, day_milli)
    # vals is a list of the of the form:
    # vals[0] = Daily Minutes Active - the number of active minutes in the current day. Always less than or equal to Total Minutes Processed. Resets when the day count increments.
    # vals[1] = Minutes Processed - the total number of minutes that have been processed in the current day. Resets when the day count increments.
    # vals[2] = Days Processed - the total number of days processed.
    # vals[3] = Points This Minute - the number of points that have been calculated in the current minute. Resets when the total minute count increments
    print "Daily Minutes Active = " + str(vals[0])
    print "Total Minutes Processed = " + str(vals[1])
    print "Total Days Processed = " + str(vals[2])
    print "Points This Minute = " + str(vals[3])


if __name__ == '__main__':
    main()
