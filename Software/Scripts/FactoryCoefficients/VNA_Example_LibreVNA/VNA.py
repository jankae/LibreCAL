# A VNA is needed to measure and create the factory coefficients.
# This file contains the necessary function to extract data from this VNA.
# Please implement all functions according to the VNA you are using.

from VNA_Example_LibreVNA.libreVNA import libreVNA
import time

vna = None

def checkIfReady() -> bool:
    """
    checkIfReady checks if the VNA is connected and ready to be used
    
    This function can also be used for initiliazing the VNA. It is called only
    once at the beginning
       
    :return: True if VNA is ready. False otherwise
    """
    global vna
    try:
        vna = libreVNA('localhost', 19542)
    except:
        # LibreVNA-GUI not detected
        print("Unable to connect to LibreVNA-GUI")
        return False
    
    if vna.query(":DEV:CONN?") == "Not connected":
        # Not connected to any LibreVNA device
        print("Not connected to any LibreVNA")
        return False

    # Make sure that the calibration is active
    if vna.query(":VNA:CAL:ACTIVE?") != "SOLT_12":
        print("LibreVNA must use SOLT_12 calibration")
        return False
    
    # Set up the sweep
    vna.cmd(":DEV:MODE VNA")
    vna.cmd(":VNA:SWEEP FREQUENCY")
    vna.cmd(":VNA:STIM:LVL -10")
    vna.cmd(":VNA:ACQ:IFBW 100")
    vna.cmd(":VNA:ACQ:AVG 1")
    vna.cmd(":VNA:ACQ:POINTS 501")
    vna.cmd(":VNA:FREQuency:START 1000000")
    vna.cmd(":VNA:FREQuency:STOP 6000000000")
    
    return True

def getPorts() -> int:
    """
    getPorts returns the number of ports the VNA has
    
    Creation of factory coefficients if faster if more ports are available.
    The VNA needs at least 2 ports and at most 4 ports can be utilized.
    
    :return: Number of ports on the VNA
    """
    return 2

def measure():
    """
    measure starts a measurement and returns the S parameter data
    
    This function should start a new measurement and block until the data
    is available. No previous measurements must be returned.
    
    Measurements are returned as a dictionary:
        Key: S parameter name (e.g. "S11")
        Value: List of tuples: [frequency, complex]
    
    :return: Measurements
    """
    
    # Trigger the sweep
    vna.cmd(":VNA:ACQ:SINGLE TRUE")
    # Wait for the sweep to finish
    while vna.query(":VNA:ACQ:FIN?") == "FALSE":
        time.sleep(0.1)
    
    ret = {}
    ret["S11"] = vna.parse_trace_data(vna.query(":VNA:TRACE:DATA? S11"))
    ret["S12"] = vna.parse_trace_data(vna.query(":VNA:TRACE:DATA? S12"))
    ret["S21"] = vna.parse_trace_data(vna.query(":VNA:TRACE:DATA? S21"))
    ret["S22"] = vna.parse_trace_data(vna.query(":VNA:TRACE:DATA? S22"))
    
    return ret
