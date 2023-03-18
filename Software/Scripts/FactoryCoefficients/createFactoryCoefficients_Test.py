import argparse
import serial
import serial.tools.list_ports
import subprocess
import time
import sys
from datetime import datetime
#import VNA
from VNA_Example_Test import VNA

def SCPICommand(ser, cmd: str) -> str:
    print("\tSCPI "+cmd)    
    ser.write((cmd+"\r\n").encode())
    resp = ser.readline().decode("ascii")
    if len(resp) == 0:
        raise Exception("Timeout occurred in communication with LibreCAL")
    if resp.strip() == "ERROR":
        raise Exception("LibreCAL returned 'ERROR' for command '"+cmd+"'")
    print("SCPI Resp="+resp)
    return resp.strip()

def SCPICommand_NoERROR_Exception(ser, cmd: str) -> str:
    print("\tSCPI "+cmd)    
    ser.write((cmd+"\r\n").encode())
    resp = ser.readline().decode("ascii")
    if len(resp) == 0:
        raise Exception("Timeout occurred in communication with LibreCAL")
    print("SCPI Resp="+resp)
    return resp.strip()

def SCPICommand_noprint(ser, cmd: str) -> str:   
    ser.write((cmd+"\r\n").encode())
    resp = ser.readline().decode("ascii")
    if len(resp) == 0:
        raise Exception("Timeout occurred in communication with LibreCAL")
    if resp.strip() == "ERROR":
        raise Exception("LibreCAL returned 'ERROR' for command '"+cmd+"'")
    return resp.strip()

parser = argparse.ArgumentParser(description = "Helps with creation of factory calibration coefficients for a new LibreCAL device")
parser.add_argument('-f', '--flash', help='Flash the firmware file first', metavar="firmware")

args = parser.parse_args()

if args.flash is not None:
    # Check if picotool is available
    if subprocess.call(['picotool', 'help'], stdout=subprocess.DEVNULL) != 0:
        raise Exception("Couldn't find picotool. Please make sure you have it installed")
               
    print("Flashing firmware...")
    if subprocess.call(['picotool', 'load', '-x', args.flash]) != 0:
        raise Exception("Flashing the firmware failed")
    
    print("Firmware flashed, waiting for device to boot")
    time.sleep(2)


# Try to find the connected LibreCAL
port = None
for p in serial.tools.list_ports.comports():
    if p.vid == 0x0483 and p.pid == 0x4122:
        port = p
        break

if port is None:
    raise Exception("No LibreCAL device detected")
    
print("Found LibreCAL device on port " + port.device)

ser = serial.Serial(port.device, timeout = 10)
idn = SCPICommand(ser, "*IDN?").split(",")
if idn[0] != "LibreCAL":
    raise Exception("Invalid *IDN response: "+idn[0])
print("Detected LibreCAL device has serial number "+idn[2])

# Enable writing of the factory partition
SCPICommand(ser, ":FACT:ENABLEWRITE I_AM_SURE")

# Clear any potential old factory values
SCPICommand(ser, ":FACT:DEL")

if not VNA.checkIfReady():
    exit("VNA is not ready.")
    
VNAports = VNA.getPorts()

if VNAports < 2:
    raise Exception("A VNA with at least 2 ports must be used")
    
if VNAports > 4:
    # Can only use up to four ports
    VNAports = 4
    
# Connection order depending on the number of available VNA ports
PortConnections = {
        2:[      # VNA has two ports
# [[VNA Port, LibreCAL port]], [[VNA Port, LibreCAL port]]
   [[1,1],[2,2]],
   [[1,1],[2,3]],
   [[1,1],[2,4]],
   [[1,2],[2,4]],
   [[1,2],[2,3]],
   [[1,4],[2,3]],
   ],
        3:[      # VNA has three ports
   [[1,1],[2,2],[3,3]],
   [[1,1],[2,2],[3,4]],
   [[1,1],[2,3],[3,4]],
   ],
        4:[      # VNA has four ports
   [[1,1],[2,2],[3,3],[4,4]],
   ],
}

Shorts = {}
Opens = {}
Loads = {}
Throughs = {}

def getTemperature(ser) -> float:
    return float(SCPICommand_noprint(ser, ":TEMP?"))

def temperatureStable(ser) -> bool:
    return SCPICommand_noprint(ser, ":TEMP:STABLE?") == "TRUE"

def sanityCheckMeasurement(measurement : dict):
    points = None
    for i in range(1,VNAports+1):
        for j in range(1,VNAports+1):
            name = "S"+str(i)+str(j)
            if not name in measurement:
                raise Exception("Measurement '"+name+"' missing in VNA measurements")
            if points is None:
                points = len(measurement[name])
            else:
                # Check if measurement has the same length
                if len(measurement[name]) != points:
                    raise Exception("All VNA measurements must have the same number of points")

def resetLibreCALPorts():
    SCPICommand(ser, ":PORT 1 NONE")
    SCPICommand(ser, ":PORT 2 NONE")
    SCPICommand(ser, ":PORT 3 NONE")
    SCPICommand(ser, ":PORT 4 NONE")

def takeMeasurements(portmapping : dict):
    global Shorts, Opens, Loads, Throughs

    if not temperatureStable(ser):
        print("Waiting for LibreCAL temperature to stabilize to 35°C...")
        date = datetime.now().strftime("%d/%m/%Y %H:%M:%S")
        temp = getTemperature(ser)
        str_temp = '{:.2f} °C\r'.format(temp)
        print("Start "+date+" "+str_temp)
        # while not temperatureStable(ser):
            # time.sleep(0.1)
            # temp = getTemperature(ser)
            # date = datetime.now().strftime("%d/%m/%Y %H:%M:%S")
            # str_temp = '{:.2f} °C\r'.format(temp)
            # sys.stdout.write(date+" "+str_temp)
            # sys.stdout.flush()
    
        date = datetime.now().strftime("%d/%m/%Y %H:%M:%S")
        print("End "+date+" "+str_temp)
    measureShorts = False
    measureOpens = False
    measureLoads = False
    for p in portmapping:
        if Shorts.get(str(p)) is None:
            measureShorts = True
        if Opens.get(str(p)) is None:
            measureOpens = True
        if Loads.get(str(p)) is None:
            measureLoads = True
    
    if measureOpens:
        print("Taking OPEN measurement...")
        resetLibreCALPorts()
        for p in portmapping:
            SCPICommand(ser, ":PORT "+str(p)+" OPEN")
        measure = VNA.measure()
        print("...measurement done.")
        sanityCheckMeasurement(measure)
        # Store measurements
        for p in portmapping:
            name = "S"+str(portmapping[p])+str(portmapping[p])
            Opens[str(p)] = measure[name]
            print("Open measurement Opens["+str(p)+"] =  measure["+name+"]")
    if measureShorts:
        print("Taking SHORT measurement...")
        resetLibreCALPorts()
        for p in portmapping:
            SCPICommand(ser, ":PORT "+str(p)+" SHORT")
        measure = VNA.measure()
        print("...measurement done.")
        sanityCheckMeasurement(measure)
        # Store measurements
        for p in portmapping:
            name = "S"+str(portmapping[p])+str(portmapping[p])
            Shorts[str(p)] = measure[name]            
            print("Short measurement Shorts["+str(p)+"] =  measure["+name+"]")
    if measureLoads:
        print("Taking LOAD measurement...")
        resetLibreCALPorts()
        for p in portmapping:
            SCPICommand(ser, ":PORT "+str(p)+" LOAD")     
        measure = VNA.measure()
        print("...measurement done.")
        sanityCheckMeasurement(measure)
        # Store measurements
        for p in portmapping:
            name = "S"+str(portmapping[p])+str(portmapping[p])
            Loads[str(p)] = measure[name]
            print("Load measurement Loads["+str(p)+"] =  measure["+name+"]")

    # Check for missing through measurements
    for p1 in portmapping:
        for p2 in portmapping:
            if p2 <= p1:
                continue
            name = str(p1)+str(p2)
            if Throughs.get(name) is None:
                # Take this through measurement
                resetLibreCALPorts()
                SCPICommand(ser, ":PORT "+str(p1)+" THROUGH "+str(p2)) 
                print("Taking THROUGH_"+name+" measurement...")
                measure = VNA.measure()
                print("...measurement done.")
                sanityCheckMeasurement(measure)
                # Store measurement
                through = {}
                through["S11"] = measure["S"+str(portmapping[p1])+str(portmapping[p1])]  
                through["S12"] = measure["S"+str(portmapping[p1])+str(portmapping[p2])]  
                through["S21"] = measure["S"+str(portmapping[p2])+str(portmapping[p1])]  
                through["S22"] = measure["S"+str(portmapping[p2])+str(portmapping[p2])]  
                Throughs[name] = through
                
    resetLibreCALPorts()

for conn in PortConnections[VNAports]:
    print("\r\nPlease make the following connections:")
    print("VNA port -> LibreCAL port")
    portMapping = {}
    for p in conn:
        print("    "+str(p[0])+"    ->      "+str(p[1]))
        portMapping[p[1]] = p[0]
    input("Press Enter to continue...")

    takeMeasurements(portMapping)
    
print("\r\nMeasurements complete.")

rCoeffs = {}
tCoeffs = {}

for p in Opens:
    rCoeffs["P"+str(p)+"_OPEN"] = Opens[p]
for p in Shorts:
    rCoeffs["P"+str(p)+"_SHORT"] = Shorts[p]
for p in Loads:
    rCoeffs["P"+str(p)+"_LOAD"] = Loads[p]
for p in Throughs:
    tCoeffs["P"+p+"_THROUGH"] = Throughs[p]
    
for r in rCoeffs:
    print("Transferring "+r+" coefficient...")
    SCPICommand(ser, ":COEFF:CREATE FACTORY "+r)
    SCPICommand(ser, ":COEFF:ADD_COMMENT comment1")
    SCPICommand(ser, ":COEFF:ADD_COMMENT Test comment 2")
    SCPICommand(ser, ":COEFF:ADD_COMMENT Test veryyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyy longgggggggggggggggggggggggggggggggggggggggggggggggggggggggggggg comment 3")
    SCPICommand(ser, ":COEFF:ADD_COMMENT Test comment 4 with tons of words with space and numbers 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16 17 18 19 20 21 22 23 24")
    SCPICommand(ser, ":COEFF:ADD_COMMENT Test comment 5 exceed max 120chars(truncated 13) with tons of words with space and numbers 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16 17 18 19 20 21 22 23 24 25 26 27 28 29 30")
    SCPICommand(ser, ":COEFF:ADD_COMMENT Test comment 6")
    SCPICommand(ser, ":COEFF:ADD_COMMENT Test comment 7")
    SCPICommand(ser, ":COEFF:ADD_COMMENT Test comment 8")
    SCPICommand(ser, ":COEFF:ADD_COMMENT Test comment 9")
    SCPICommand(ser, ":COEFF:ADD_COMMENT Test comment 10")
    SCPICommand(ser, ":COEFF:ADD_COMMENT Test comment 11")
    SCPICommand(ser, ":COEFF:ADD_COMMENT Test comment 12")
    SCPICommand(ser, ":COEFF:ADD_COMMENT a b c d e f g h i j k l m n o p q r s t u v w x y z A B C D E F G H I J K L M N O P Q R S T U V W X Y Z 0 1 2 3 4 5 6 7 8 9 A B C D E F")   
    SCPICommand(ser, ":COEFF:ADD_COMMENT !\"#$%&'()*+,-./0123456789:;<=>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\]^_`abcdefghijklmnopqrstuvwxyz{|}~")
    SCPICommand(ser, ":COEFF:ADD_COMMENT Test comment 15")
    SCPICommand(ser, ":COEFF:ADD_COMMENT Test comment 16")
    SCPICommand(ser, ":COEFF:ADD_COMMENT Test comment 17")
    SCPICommand(ser, ":COEFF:ADD_COMMENT Test comment 18")
    SCPICommand(ser, ":COEFF:ADD_COMMENT Test comment 19")
    SCPICommand(ser, ":COEFF:ADD_COMMENT Test comment 20")
    SCPICommand(ser, ":COEFF:ADD_COMMENT Test comment 21")
    SCPICommand(ser, ":COEFF:ADD_COMMENT Test comment 22")
    SCPICommand(ser, ":COEFF:ADD_COMMENT Test comment 23")
    SCPICommand(ser, ":COEFF:ADD_COMMENT Test comment 24")
    SCPICommand(ser, ":COEFF:ADD_COMMENT Test comment 25")
    SCPICommand(ser, ":COEFF:ADD_COMMENT Test comment 26")
    SCPICommand(ser, ":COEFF:ADD_COMMENT Test comment 27")
    SCPICommand(ser, ":COEFF:ADD_COMMENT Test comment 28")
    SCPICommand(ser, ":COEFF:ADD_COMMENT Test comment 29")
    SCPICommand(ser, ":COEFF:ADD_COMMENT Test comment 30")
    SCPICommand(ser, ":COEFF:ADD_COMMENT Test comment 31")
    SCPICommand(ser, ":COEFF:ADD_COMMENT Test comment 32")
    SCPICommand(ser, ":COEFF:ADD_COMMENT Test comment 33")
    SCPICommand(ser, ":COEFF:ADD_COMMENT Test comment 34")
    SCPICommand(ser, ":COEFF:ADD_COMMENT Test comment 35")
    SCPICommand(ser, ":COEFF:ADD_COMMENT Test comment 36")
    SCPICommand(ser, ":COEFF:ADD_COMMENT Test comment 37")
    SCPICommand(ser, ":COEFF:ADD_COMMENT Test comment 38")
    SCPICommand(ser, ":COEFF:ADD_COMMENT Test comment 39")
    SCPICommand(ser, ":COEFF:ADD_COMMENT Test comment 40")
    SCPICommand(ser, ":COEFF:ADD_COMMENT Test comment 41")
    SCPICommand(ser, ":COEFF:ADD_COMMENT Test comment 42")
    SCPICommand(ser, ":COEFF:ADD_COMMENT Test comment 43")
    SCPICommand(ser, ":COEFF:ADD_COMMENT Test comment 44")
    SCPICommand(ser, ":COEFF:ADD_COMMENT Test comment 45")
    SCPICommand(ser, ":COEFF:ADD_COMMENT Test comment 46")
    SCPICommand(ser, ":COEFF:ADD_COMMENT Test comment 47")
    SCPICommand(ser, ":COEFF:ADD_COMMENT Test comment 48")
    SCPICommand(ser, ":COEFF:ADD_COMMENT Test comment 49")
    SCPICommand(ser, ":COEFF:ADD_COMMENT Test comment 50")
    SCPICommand(ser, ":COEFF:ADD_COMMENT Test comment 51")
    SCPICommand(ser, ":COEFF:ADD_COMMENT Test comment 52")
    SCPICommand(ser, ":COEFF:ADD_COMMENT Test comment 53")
    SCPICommand(ser, ":COEFF:ADD_COMMENT Test comment 54")
    SCPICommand(ser, ":COEFF:ADD_COMMENT Test comment 55")
    SCPICommand(ser, ":COEFF:ADD_COMMENT Test comment 56")
    SCPICommand(ser, ":COEFF:ADD_COMMENT Test comment 57")
    SCPICommand(ser, ":COEFF:ADD_COMMENT Test comment 58")
    SCPICommand(ser, ":COEFF:ADD_COMMENT Test comment 59")
    SCPICommand(ser, ":COEFF:ADD_COMMENT Test comment 60")
    SCPICommand(ser, ":COEFF:ADD_COMMENT Test comment 61")
    SCPICommand(ser, ":COEFF:ADD_COMMENT Test comment 62")
    SCPICommand(ser, ":COEFF:ADD_COMMENT Test comment 63")
    SCPICommand(ser, ":COEFF:ADD_COMMENT Test comment 64")
    SCPICommand(ser, ":COEFF:ADD_COMMENT Test comment 65")
    SCPICommand(ser, ":COEFF:ADD_COMMENT Test comment 66")
    SCPICommand(ser, ":COEFF:ADD_COMMENT Test comment 67")
    SCPICommand(ser, ":COEFF:ADD_COMMENT Test comment 68")
    SCPICommand(ser, ":COEFF:ADD_COMMENT Test comment 69")
    SCPICommand(ser, ":COEFF:ADD_COMMENT Test comment 70")
    SCPICommand(ser, ":COEFF:ADD_COMMENT Test comment 71")
    SCPICommand(ser, ":COEFF:ADD_COMMENT Test comment 72")
    SCPICommand(ser, ":COEFF:ADD_COMMENT Test comment 73")
    SCPICommand(ser, ":COEFF:ADD_COMMENT Test comment 74")
    SCPICommand(ser, ":COEFF:ADD_COMMENT Test comment 75")
    SCPICommand(ser, ":COEFF:ADD_COMMENT Test comment 76")
    SCPICommand(ser, ":COEFF:ADD_COMMENT Test comment 77")
    SCPICommand(ser, ":COEFF:ADD_COMMENT Test comment 78")
    SCPICommand(ser, ":COEFF:ADD_COMMENT Test comment 79")
    SCPICommand(ser, ":COEFF:ADD_COMMENT Test comment 80")
    SCPICommand(ser, ":COEFF:ADD_COMMENT Test comment 81")
    SCPICommand(ser, ":COEFF:ADD_COMMENT Test comment 82")
    SCPICommand(ser, ":COEFF:ADD_COMMENT Test comment 83")
    SCPICommand(ser, ":COEFF:ADD_COMMENT Test comment 84")
    SCPICommand(ser, ":COEFF:ADD_COMMENT Test comment 85")
    SCPICommand(ser, ":COEFF:ADD_COMMENT Test comment 86")
    SCPICommand(ser, ":COEFF:ADD_COMMENT Test comment 87")
    SCPICommand(ser, ":COEFF:ADD_COMMENT Test comment 88")
    SCPICommand(ser, ":COEFF:ADD_COMMENT Test comment 89")
    SCPICommand(ser, ":COEFF:ADD_COMMENT Test comment 90")
    SCPICommand(ser, ":COEFF:ADD_COMMENT Test comment 91")
    SCPICommand(ser, ":COEFF:ADD_COMMENT Test comment 92")
    SCPICommand(ser, ":COEFF:ADD_COMMENT Test comment 93")
    SCPICommand(ser, ":COEFF:ADD_COMMENT Test comment 94")
    SCPICommand(ser, ":COEFF:ADD_COMMENT Test comment 95")
    SCPICommand(ser, ":COEFF:ADD_COMMENT Test comment 96")
    SCPICommand(ser, ":COEFF:ADD_COMMENT Test comment 97")
    SCPICommand(ser, ":COEFF:ADD_COMMENT Test comment 98")
    SCPICommand(ser, ":COEFF:ADD_COMMENT Test comment 99")
    SCPICommand(ser, ":COEFF:ADD_COMMENT Test comment 100")
    SCPICommand_NoERROR_Exception(ser, ":COEFF:ADD_COMMENT Test comment 101 shall be not written and shall return an ERROR it exceed 100 max comment per file")
    for data in rCoeffs[r]:
        SCPICommand(ser, ":COEFF:ADD "+str(data[0] / 1000000000.0)+" "+str(data[1].real)+" "+str(data[1].imag))
    # Check error happen
    SCPICommand_NoERROR_Exception(ser, ":COEFF:ADD_COMMENT forbidden comment shall not be present in file")
    SCPICommand(ser, ":COEFF:FIN")

for t in tCoeffs:
    print("Transferring "+t+" coefficient...")
    SCPICommand(ser, ":COEFF:CREATE FACTORY "+t)
    SCPICommand(ser, ":COEFF:ADD_COMMENT Test comment 1")
    SCPICommand(ser, ":COEFF:ADD_COMMENT Test veryyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyy longgggggggggggggggggggggggggggggggggggggggggggggggggggggggggggg comment 2")   
    SCPICommand(ser, ":COEFF:ADD_COMMENT Test comment 3 with tons of words with space and numbers 1 2 3 4 ")
    SCPICommand(ser, ":COEFF:ADD_COMMENT Test comment 4 with some words and spaces")
    SCPICommand(ser, ":COEFF:ADD_COMMENT Test comment 5 exceed max 120chars(truncated 13) with tons of words with space and numbers 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16 17 18 19 20 21 22 23 24 25 26 27 28 29 30")
    SCPICommand(ser, ":COEFF:ADD_COMMENT a b c d e f g h i j k l m n o p q r s t u v w x y z A B C D E F G H I J K L M N O P Q R S T U V W X Y Z 0 1 2 3 4 5 6 7 8 9 A B C D E F")
    SCPICommand(ser, ":COEFF:ADD_COMMENT !\"#$%&'()*+,-./0123456789:;<=>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\]^_`abcdefghijklmnopqrstuvwxyz{|}~")
    SCPICommand(ser, ":COEFF:ADD_COMMENT Test comment 8")
    SCPICommand(ser, ":COEFF:ADD_COMMENT Test comment 9")
    SCPICommand(ser, ":COEFF:ADD_COMMENT Test comment 10")
    SCPICommand(ser, ":COEFF:ADD_COMMENT Test comment 11")
    SCPICommand(ser, ":COEFF:ADD_COMMENT Test comment 12")
    SCPICommand(ser, ":COEFF:ADD_COMMENT Test comment 13")
    SCPICommand(ser, ":COEFF:ADD_COMMENT Test comment 14")
    SCPICommand(ser, ":COEFF:ADD_COMMENT Test comment 15")
    SCPICommand(ser, ":COEFF:ADD_COMMENT Test comment 16")
    SCPICommand(ser, ":COEFF:ADD_COMMENT Test comment 17")
    SCPICommand(ser, ":COEFF:ADD_COMMENT Test comment 18")
    SCPICommand(ser, ":COEFF:ADD_COMMENT Test comment 19")
    SCPICommand(ser, ":COEFF:ADD_COMMENT Test comment 20")
    SCPICommand(ser, ":COEFF:ADD_COMMENT Test comment 21")
    SCPICommand(ser, ":COEFF:ADD_COMMENT Test comment 22")
    SCPICommand(ser, ":COEFF:ADD_COMMENT Test comment 23")
    SCPICommand(ser, ":COEFF:ADD_COMMENT Test comment 24")
    SCPICommand(ser, ":COEFF:ADD_COMMENT Test comment 25")
    SCPICommand(ser, ":COEFF:ADD_COMMENT Test comment 26")
    SCPICommand(ser, ":COEFF:ADD_COMMENT Test comment 27")
    SCPICommand(ser, ":COEFF:ADD_COMMENT Test comment 28")
    SCPICommand(ser, ":COEFF:ADD_COMMENT Test comment 29")
    SCPICommand(ser, ":COEFF:ADD_COMMENT Test comment 30")
    SCPICommand(ser, ":COEFF:ADD_COMMENT Test comment 31")
    SCPICommand(ser, ":COEFF:ADD_COMMENT Test comment 32")
    SCPICommand(ser, ":COEFF:ADD_COMMENT Test comment 33")
    SCPICommand(ser, ":COEFF:ADD_COMMENT Test comment 34")
    SCPICommand(ser, ":COEFF:ADD_COMMENT Test comment 35")
    SCPICommand(ser, ":COEFF:ADD_COMMENT Test comment 36")
    SCPICommand(ser, ":COEFF:ADD_COMMENT Test comment 37")
    SCPICommand(ser, ":COEFF:ADD_COMMENT Test comment 38")
    SCPICommand(ser, ":COEFF:ADD_COMMENT Test comment 39")
    SCPICommand(ser, ":COEFF:ADD_COMMENT Test comment 40")
    SCPICommand(ser, ":COEFF:ADD_COMMENT Test comment 41")
    SCPICommand(ser, ":COEFF:ADD_COMMENT Test comment 42")
    SCPICommand(ser, ":COEFF:ADD_COMMENT Test comment 43")
    SCPICommand(ser, ":COEFF:ADD_COMMENT Test comment 44")
    SCPICommand(ser, ":COEFF:ADD_COMMENT Test comment 45")
    SCPICommand(ser, ":COEFF:ADD_COMMENT Test comment 46")
    SCPICommand(ser, ":COEFF:ADD_COMMENT Test comment 47")
    SCPICommand(ser, ":COEFF:ADD_COMMENT Test comment 48")
    SCPICommand(ser, ":COEFF:ADD_COMMENT Test comment 49")
    SCPICommand(ser, ":COEFF:ADD_COMMENT Test comment 50")
    SCPICommand(ser, ":COEFF:ADD_COMMENT Test comment 51")
    SCPICommand(ser, ":COEFF:ADD_COMMENT Test comment 52")
    SCPICommand(ser, ":COEFF:ADD_COMMENT Test comment 53")
    SCPICommand(ser, ":COEFF:ADD_COMMENT Test comment 54")
    SCPICommand(ser, ":COEFF:ADD_COMMENT Test comment 55")
    SCPICommand(ser, ":COEFF:ADD_COMMENT Test comment 56")
    SCPICommand(ser, ":COEFF:ADD_COMMENT Test comment 57")
    SCPICommand(ser, ":COEFF:ADD_COMMENT Test comment 58")
    SCPICommand(ser, ":COEFF:ADD_COMMENT Test comment 59")
    SCPICommand(ser, ":COEFF:ADD_COMMENT Test comment 60")
    SCPICommand(ser, ":COEFF:ADD_COMMENT Test comment 61")
    SCPICommand(ser, ":COEFF:ADD_COMMENT Test comment 62")
    SCPICommand(ser, ":COEFF:ADD_COMMENT Test comment 63")
    SCPICommand(ser, ":COEFF:ADD_COMMENT Test comment 64")
    SCPICommand(ser, ":COEFF:ADD_COMMENT Test comment 65")
    SCPICommand(ser, ":COEFF:ADD_COMMENT Test comment 66")
    SCPICommand(ser, ":COEFF:ADD_COMMENT Test comment 67")
    SCPICommand(ser, ":COEFF:ADD_COMMENT Test comment 68")
    SCPICommand(ser, ":COEFF:ADD_COMMENT Test comment 69")
    SCPICommand(ser, ":COEFF:ADD_COMMENT Test comment 70")
    SCPICommand(ser, ":COEFF:ADD_COMMENT Test comment 71")
    SCPICommand(ser, ":COEFF:ADD_COMMENT Test comment 72")
    SCPICommand(ser, ":COEFF:ADD_COMMENT Test comment 73")
    SCPICommand(ser, ":COEFF:ADD_COMMENT Test comment 74")
    SCPICommand(ser, ":COEFF:ADD_COMMENT Test comment 75")
    SCPICommand(ser, ":COEFF:ADD_COMMENT Test comment 76")
    SCPICommand(ser, ":COEFF:ADD_COMMENT Test comment 77")
    SCPICommand(ser, ":COEFF:ADD_COMMENT Test comment 78")
    SCPICommand(ser, ":COEFF:ADD_COMMENT Test comment 79")
    SCPICommand(ser, ":COEFF:ADD_COMMENT Test comment 80")
    SCPICommand(ser, ":COEFF:ADD_COMMENT Test comment 81")
    SCPICommand(ser, ":COEFF:ADD_COMMENT Test comment 82")
    SCPICommand(ser, ":COEFF:ADD_COMMENT Test comment 83")
    SCPICommand(ser, ":COEFF:ADD_COMMENT Test comment 84")
    SCPICommand(ser, ":COEFF:ADD_COMMENT Test comment 85")
    SCPICommand(ser, ":COEFF:ADD_COMMENT Test comment 86")
    SCPICommand(ser, ":COEFF:ADD_COMMENT Test comment 87")
    SCPICommand(ser, ":COEFF:ADD_COMMENT Test comment 88")
    SCPICommand(ser, ":COEFF:ADD_COMMENT Test comment 89")
    SCPICommand(ser, ":COEFF:ADD_COMMENT Test comment 90")
    SCPICommand(ser, ":COEFF:ADD_COMMENT Test comment 91")
    SCPICommand(ser, ":COEFF:ADD_COMMENT Test comment 92")
    SCPICommand(ser, ":COEFF:ADD_COMMENT Test comment 93")
    SCPICommand(ser, ":COEFF:ADD_COMMENT Test comment 94")
    SCPICommand(ser, ":COEFF:ADD_COMMENT Test comment 95")
    SCPICommand(ser, ":COEFF:ADD_COMMENT Test comment 96")
    SCPICommand(ser, ":COEFF:ADD_COMMENT Test comment 97")
    SCPICommand(ser, ":COEFF:ADD_COMMENT Test comment 98")
    SCPICommand(ser, ":COEFF:ADD_COMMENT Test comment 99")
    SCPICommand(ser, ":COEFF:ADD_COMMENT Test comment 100")
    SCPICommand_NoERROR_Exception(ser, ":COEFF:ADD_COMMENT Test comment 101 shall be not written and shall return an ERROR it exceed 100 max comment per file")
    SCPICommand_NoERROR_Exception(ser, ":COEFF:ADD_COMMENT Test comment 102 shall be not written and shall return an ERROR it exceed 100 max comment per file")
    m = tCoeffs[t]
    length = len(m["S11"])
    for i in range(length):       
        SCPICommand(ser, ":COEFF:ADD "+str(m["S11"][i][0] / 1000000000.0)
            +" "+str(m["S11"][i][1].real)+" "+str(m["S11"][i][1].imag)
            +" "+str(m["S21"][i][1].real)+" "+str(m["S21"][i][1].imag)
            +" "+str(m["S12"][i][1].real)+" "+str(m["S12"][i][1].imag)
            +" "+str(m["S22"][i][1].real)+" "+str(m["S22"][i][1].imag))
    SCPICommand(ser, ":COEFF:FIN")
    
print("...done, please remove LibreCAL device")
