import argparse
import serial
import serial.tools.list_ports
import subprocess
import time
#import VNA
from VNA_Example_LibreVNA import VNA

def SCPICommand(ser, cmd: str) -> str:
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

ser = serial.Serial(port.device, timeout = 1)
idn = SCPICommand(ser, "*IDN?").split("_")
if idn[0] != "LibreCAL":
    raise Exception("Invalid *IDN response: "+idn[0])
print("Detected LibreCAL device has serial number "+idn[1])

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

def temperatureStable(ser) -> bool:
    return SCPICommand(ser, ":TEMP:STABLE?") == "TRUE"

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
        print("Waiting for LibreCAL temperature to stabilize...")
        while not temperatureStable(ser):
            time.sleep(0.1)
    
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
        resetLibreCALPorts()
        for p in portmapping:
            SCPICommand(ser, ":PORT "+str(p)+" OPEN")
        print("Taking OPEN measurement...")
        measure = VNA.measure()
        print("...measurement done.")
        sanityCheckMeasurement(measure)
        # Store measurements
        for p in portmapping:
            name = "S"+str(portmapping[p])+str(portmapping[p])
            Opens[str(p)] = measure[name]            

    if measureShorts:
        resetLibreCALPorts()
        for p in portmapping:
            SCPICommand(ser, ":PORT "+str(p)+" SHORT")
        print("Taking SHORT measurement...")
        measure = VNA.measure()
        print("...measurement done.")
        sanityCheckMeasurement(measure)
        # Store measurements
        for p in portmapping:
            name = "S"+str(portmapping[p])+str(portmapping[p])
            Shorts[str(p)] = measure[name]            

    if measureLoads:
        resetLibreCALPorts()
        for p in portmapping:
            SCPICommand(ser, ":PORT "+str(p)+" LOAD")
        print("Taking LOAD measurement...")
        measure = VNA.measure()
        print("...measurement done.")
        sanityCheckMeasurement(measure)
        # Store measurements
        for p in portmapping:
            name = "S"+str(portmapping[p])+str(portmapping[p])
            Loads[str(p)] = measure[name]       
            
    # Check for missing through measurements
    for p1 in portmapping:
        for p2 in portmapping:
            if p2 <= p1:
                continue
            name = str(p1)+str(p2)
            if Throughs.get(name) is None:
                # Take this through measurement
                resetLibreCALPorts()
                SCPICommand(ser, ":PORT "+str(p1)+" THROUGH")
                SCPICommand(ser, ":PORT "+str(p2)+" THROUGH")
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
    for data in rCoeffs[r]:
        SCPICommand(ser, ":COEFF:ADD "+str(data[0] / 1000000000.0)+" "+str(data[1].real)+" "+str(data[1].imag))
    SCPICommand(ser, ":COEFF:FIN")

for t in tCoeffs:
    print("Transferring "+t+" coefficient...")
    SCPICommand(ser, ":COEFF:CREATE FACTORY "+t)
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
