import argparse
import serial
import serial.tools.list_ports
import subprocess
import time
import json
import math
import cmath
from datetime import datetime
import os
import shutil
from ftplib import FTP
#import VNA
#from VNA_Example_LibreVNA import VNA
from VNA_Example_SNA5000A import VNA

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
parser.add_argument('-l', '--limits', help='Enables limit checking on coefficients with limits from json file', metavar="json_limit_file")
parser.add_argument('-d', '--directory', help='Save measured touchstone files to this path', metavar="touchstone_path")
parser.add_argument('--ftp', help='Save measured touchstone files to a FTP server (requires --directory)', metavar="ftp_json_file")

args = parser.parse_args()

if args.ftp and not args.directory:
    raise Exception("ftp option is only valid when directory option is also specified")

if args.flash is not None:
    # Check if picotool is available
    if subprocess.call(['picotool', 'help'], stdout=subprocess.DEVNULL) != 0:
        raise Exception("Couldn't find picotool. Please make sure you have it installed")
               
    print("Flashing firmware...")
    if subprocess.call(['picotool', 'load', '-x', args.flash]) != 0:
        raise Exception("Flashing the firmware failed")
    
    print("Firmware flashed, waiting for device to boot")
    time.sleep(4)

# Try to find the connected LibreCAL
port = None
for p in serial.tools.list_ports.comports():
    if (p.vid == 0x0483 and p.pid == 0x4122) or (p.vid == 0x1209 and p.pid == 0x4122):
        port = p
        break

if port is None:
    raise Exception("No LibreCAL device detected")
    
print("Found LibreCAL device on port " + port.device)

ser = serial.Serial(port.device, timeout = 2)
idn = SCPICommand(ser, "*IDN?").split(",")
if idn[0] != "LibreCAL":
    raise Exception("Invalid *IDN response: "+idn)
libreCAL_serial = idn[2]
libreCAL_firmware = idn[3]
print("Detected LibreCAL device has serial number "+libreCAL_serial)

# Set the time on the LibreCAL
dt = datetime.now()
now_utc = datetime.utcnow()
utc_offset = dt - now_utc
offset_seconds = int(utc_offset.total_seconds())
offset_hours = abs(offset_seconds // 3600)
offset_minutes = abs((offset_seconds // 60) % 60)
if offset_seconds < 0:
    sign = "-"
else:
    sign = "+"
offset_str = f"{sign}{offset_hours:02d}:{offset_minutes:02d}"
dt_str = dt.strftime("%Y/%m/%d %H:%M:%S")
dt_str_with_offset = f"{dt_str} UTC{offset_str}"
SCPICommand(ser, ":DATE_TIME "+dt_str_with_offset)

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

    start = datetime.now()
    if not temperatureStable(ser):
        print("Waiting for LibreCAL temperature to stabilize...")
        while not temperatureStable(ser):
            time.sleep(0.1)
            if (datetime.now() - start).total_seconds() > 180:
                raise Exception("Timed out while waiting for temperature of LibreCAL to stabilize")
    
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

if args.directory:
    # Store measured parameter in files

    # Save result on disk
    try:
        shutil.rmtree(args.directory + "/" + libreCAL_serial)
    except:
        pass
    try:
        os.remove(args.directory + "/" + libreCAL_serial + ".zip")
    except:
        pass

    os.mkdir(args.directory + "/" + libreCAL_serial)
    f = open(args.directory + "/" + libreCAL_serial + "/info.txt", "w")
    f.write("Factory calibration data for LibreCAL\n")
    f.write("Serial number: " + libreCAL_serial + "\n")
    f.write("Firmware version: " + libreCAL_firmware + "\n")
    f.write("Timestamp: " + datetime.now().isoformat() + "\n")
    f.write("\n")
    f.write("Calibration equipment:\n")
    f.write(VNA.getInfo() + "\n")
    for r in rCoeffs:
        f = open(args.directory + "/" + libreCAL_serial + "/" + r + ".s1p", "w")
        f.write("! Created by factory calibration script\n")
        f.write("! "+datetime.now().isoformat()+"\n")
        f.write("# GHz S RI R 50.0\n")
        for data in rCoeffs[r]:
            f.write(str(data[0] / 1000000000.0) + " " + str(data[1].real) + " " + str(data[1].imag) + "\n")
        f.close()
    for t in tCoeffs:
        f = open(args.directory + "/" + libreCAL_serial + "/" + t + ".s2p", "w")
        f.write("! Created by factory calibration script\n")
        f.write("! "+datetime.now().isoformat()+"\n")
        f.write("# GHz S RI R 50.0\n")
        m = tCoeffs[t]
        length = len(m["S11"])
        for i in range(length):
            f.write(str(m["S11"][i][0] / 1000000000.0)
                    + " " + str(m["S11"][i][1].real) + " " + str(m["S11"][i][1].imag)
                    + " " + str(m["S21"][i][1].real) + " " + str(m["S21"][i][1].imag)
                    + " " + str(m["S12"][i][1].real) + " " + str(m["S12"][i][1].imag)
                    + " " + str(m["S22"][i][1].real) + " " + str(m["S22"][i][1].imag) + "\n")
        f.close()
    # zip and delete uncompressed
    shutil.make_archive(args.directory + "/" + libreCAL_serial, 'zip', args.directory + "/" + libreCAL_serial)
    shutil.rmtree(args.directory + "/" + libreCAL_serial)


if args.limits:
    jlimits = None
    try:
        f = open(args.limits)
        jlimits = json.load(f)
    except Exception as e:
        raise Exception("Failed to parse limit file")
    
   
    for i in jlimits:
        # grab the specified measurement
        measurements = {}
        if i == "OPEN":
            measurements = Opens
        elif i == "SHORT":
            measurements = Shorts
        elif i == "LOAD":
            measurements = Loads
        elif i == "THRU REFLECTION":
            for through in Throughs:
                measurements[through + "_S11"] = Throughs[through]["S11"]
                measurements[through + "_S22"] = Throughs[through]["S22"]
        elif i == "THRU TRANSMISSION":
            for through in Throughs:
                measurements[through + "_S12"] = Throughs[through]["S12"]
                measurements[through + "_S21"] = Throughs[through]["S21"]
        elif i == "OPEN SHORT PHASE":
            for key in Opens.keys():
                if key not in Shorts:
                    # should not happen
                    raise RuntimeError(
                        "Got an open measurement without corresponding short measurement at port " + str(key))
                samples = max(len(Opens[key]), len(Shorts[key]))
                open_vs_short = []
                for j in range(samples):
                    if Opens[key][j][0] == Shorts[key][j][0]:
                        # this sample uses the same frequency (should always be the case)
                        open_vs_short.append((Opens[key][j][0], Opens[key][j][1] / Shorts[key][j][1]))
                    else:
                        raise RuntimeError(
                            "Open and short measurements have difference frequencies at port " + str(key))
                measurements[key] = open_vs_short

        if len(measurements) == 0:
            # unknown limit keyword, nothing to check
            continue

        # iterate over and check the specified limits
        for limit in jlimits[i]:
            # iterate over the measurements we need to check
            for key in measurements.keys():
                if "applicable_to" in limit:
                    # this limit only applies to some through measurements
                    meas_name = key[0:2]
                    #print(limit, meas_name)
                    if not meas_name in limit["applicable_to"]:
                        # not applicable for this through measurements
                        continue
                # check every sample in the measurement

                # keep track of unwrapped phase for delay limits
                unwrapped_phase = cmath.phase(measurements[key][0][1])
                if i == "SHORT":
                    # phase for short standards is inverted
                    unwrapped_phase = unwrapped_phase + math.pi
                    if unwrapped_phase > math.pi:
                        unwrapped_phase = unwrapped_phase - 2 * math.pi

                for sample in measurements[key]:
                    # update unwrapped phase
                    phase = cmath.phase(sample[1])
                    if i == "SHORT":
                        # phase for short standards is inverted
                        phase = phase + math.pi
                    while phase - math.pi > unwrapped_phase:
                        phase = phase - 2 * math.pi
                    unwrapped_phase = phase

                    if sample[0] < limit["x1"] or sample[0] > limit["x2"]:
                        # Sample not covered by this limit
                        continue
                    # calculate limit value for this sample
                    alpha = (sample[0] - limit["x1"]) / (limit["x2"] - limit["x1"])
                    limval = limit["y1"] + alpha * (limit["y2"] - limit["y1"])

                    # transform y value according to limit type
                    yval = None
                    if limit["type"] == "dB":
                        yval = 20 * math.log10(abs(sample[1]))
                    elif limit["type"] == "phase":
                        yval = math.degrees(cmath.phase(sample[1]))
                        # contrain to [0, 360)
                        while yval < 0:
                            yval += 360
                        while yval >= 360:
                            yval -= 360
                    elif limit["type"] == "delay":
                        yval = -unwrapped_phase / (2 * math.pi) / sample[0]
                    else:
                        # unknown limit type
                        raise Exception("Unknown limit type: " + str(limit["type"]))

                    # perform the actual limit check
                    success = True
                    if limit["limit"] == "max":
                        if yval > limval:
                            success = False
                    elif limit["limit"] == "min":
                        if yval < limval:
                            success = False
                    else:
                        # unknown limit
                        raise Exception("Unknown limit: " + str(limit["limit"]))
                    if not success:
                        # this limit failed
                        raise Exception("Limit check ("+limit["type"]+") failed for type " + str(i) + " in measurement "
                                        + str(key) + " at frequency " + str(sample[0]) + ": limit is " + str(
                                        limval) + ", measured value is " + str(yval))

# Limits are valid, upload to FTP server at this point
if args.ftp:
    print("Uploading to FTP server...")
    jftp = None
    try:
        f = open(args.ftp)
        jftp = json.load(f)
    except Exception as e:
        raise Exception("Failed to parse FTP config file: "+str(e))
        
    try:
        ftp = FTP(host=jftp["host"], user=jftp["user"], passwd=jftp["password"])
        # Open the previously created zip file
        zipname = libreCAL_serial + '.zip'
        zippath = args.directory + "/" + zipname
        zipfile = open(zippath, 'rb')
        ftp.storbinary("STOR "+zipname, zipfile)
        zipfile.close()
        ftp.quit()
    except Exception as e:
        raise Exception("FTP transfer failed: "+str(e))
        
# Enable writing of the factory partition
SCPICommand(ser, ":FACT:ENABLEWRITE I_AM_SURE")

# Clear any potential old factory values
SCPICommand(ser, ":FACT:DEL")

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
