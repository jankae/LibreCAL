# A VNA is needed to measure and create the factory coefficients.
# This file contains the necessary function to extract data from this VNA.
# Please implement all functions according to the VNA you are using.

from VNA_Example_Test.Test import Test
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
        vna = Test('localhost', 19542)
    except:
        # Test-GUI not detected
        print("Unable to connect to Test-GUI")
        return False
    """
    checkIfReady checks if the VNA is connected and ready to be used
    
    This function can also be used for initiliazing the VNA. It is called only
    once at the beginning
       
    :return: True if VNA is ready. False otherwise
    """   
    return True

def getPorts() -> int:
    """
    getPorts returns the number of ports the VNA has
    
    Creation of factory coefficients if faster if more ports are available.
    The VNA needs at least 2 ports and at most 4 ports can be utilized.
    
    :return: Number of ports on the VNA
    """
    return 2

def measure_set_nb_points(nb_points):
    """
    For test purpose set number of points to create during measure()
    """
    vna.measure_set_nb_points(nb_points)
    return

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
    # while vna.query(":VNA:ACQ:FIN?") == "FALSE":
        # time.sleep(0.1)
    
    ret = {}
    ret["S11"] = vna.parse_trace_data(vna.query(":VNA:TRACE:DATA? S11"))
    ret["S12"] = vna.parse_trace_data(vna.query(":VNA:TRACE:DATA? S12"))
    ret["S21"] = vna.parse_trace_data(vna.query(":VNA:TRACE:DATA? S21"))
    ret["S22"] = vna.parse_trace_data(vna.query(":VNA:TRACE:DATA? S22"))
    
    return ret
