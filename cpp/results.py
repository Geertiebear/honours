import sys
from parse import *

read_time = 10
write_time = 100
adc_latency = 1

total_time = 0

file = sys.argv[1]
with open(file, "r") as results:
    data = results.readlines()
    [row, col] = parse("init: {},{}", data[0]) 
    for i in range(1, len(data)):
        parsed = parse("{:l}{}", data[i])
        command = parsed[0].strip('\n')
        if command == "clear":
            print("clear")
        elif command == "read":
            [adc_acts, row_reads, row_writes, inputs] = parse(" {:d},{:d},{:d},{:d}\n", parsed[1])
            total_time += (row_reads * read_time + adc_acts * adc_latency) * inputs
        elif command == "write":
            [adc_acts, row_reads, row_writes, inputs] = parse(" {:d},{:d},{:d},{:d}\n", parsed[1])
            total_time += (row_writes * write_time + adc_acts * adc_latency)
        else:
            print("Unknown command " + command)

print("Total time: " + str(total_time))
