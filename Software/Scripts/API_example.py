#!/usr/bin/env python3

from libreCAL import libreCAL

# Connect to the first detected LibreCAL
cal = libreCAL()
print("Connected to LibreCAL with serial "+cal.getSerial())

# Reset all ports
cal.reset()

# Set some port standards
cal.setPort(cal.Standard.LOAD, 1)
cal.setPort(cal.Standard.SHORT, 2)
cal.setPort(cal.Standard.THROUGH, 3, 4)

# Readback
for i in range(1,5):
    print("Port "+str(i)+" is set to "+cal.getPort(i).name)

# Show temperature state
temp = cal.getTemperature()
if cal.isStable():
    print("The temperature ("+str(temp)+") is stable.")
else:
    print("The temperature ("+str(temp)+") is unstable.")

print("The heater currently uses "+str(cal.getHeaterPower())+"W.")
