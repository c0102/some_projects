import os 

lines = []
with open(r"D:\Enchele\Projects\FO_Control_Platform\docs\uart_log.txt", "r") as log_file:
    for line in log_file.readlines():
        if line.startswith("temp") or line.startswith("D") or line.startswith("H2D"):
            continue  
        else:
            lines.append(line)
with open(r"D:\Enchele\Projects\FO_Control_Platform\docs\uart_log.txt", 'w') as  log_file:
    
    for line  in lines:
        writable = "{},\n".format(line[:-1])
        log_file.write(writable)

