#!/usr/bin/python
# -*- coding: utf-8 -*-

import pygame

pygame.init()

# Loop until the user clicks the close button.
done = False

# Used to manage how fast the screen updates
clock = pygame.time.Clock()

# Initialize the joysticks
pygame.joystick.init()

# -------- Main Program Loop -----------
while done==False:
    # EVENT PROCESSING STEP
    for event in pygame.event.get(): # User did something
        if event.type == pygame.QUIT: # If user clicked close
            done=True # Flag that we are done so we exit this loop

        # Possible joystick actions: JOYAXISMOTION JOYBALLMOTION JOYBUTTONDOWN JOYBUTTONUP JOYHATMOTION
        if event.type == pygame.JOYBUTTONDOWN:
            print("Joystick button pressed.")
        if event.type == pygame.JOYBUTTONUP:
            print("Joystick button released.")


    # Get count of joysticks
    joystick_count = pygame.joystick.get_count()

    print("Number of joysticks: {}".format(joystick_count) )

    # For each joystick:
    for i in range(joystick_count):
        joystick = pygame.joystick.Joystick(i)
        joystick.init()

        print("Joystick {}".format(i) )

        # Get the name from the OS for the controller/joystick
        name = joystick.get_name()
        print("Joystick name: {}".format(name) )

        # Usually axis run in pairs, up/down for one, and left/right for
        # the other.
        axes = joystick.get_numaxes()
        print("Number of axes: {}".format(axes) )

        for i in range( axes ):
            axis = joystick.get_axis( i )
            print("Axis {} value: {:>6.3f}".format(i, axis) )

        buttons = joystick.get_numbuttons()
        print("Number of buttons: {}".format(buttons) )

        for i in range( buttons ):
            button = joystick.get_button( i )
            print("Button {:>2} value: {}".format(i,button) )

        # Hat switch. All or nothing for direction, not like joysticks.
        # Value comes back in an array.
        hats = joystick.get_numhats()
        print("Number of hats: {}".format(hats) )

        for i in range( hats ):
            hat = joystick.get_hat( i )
            print("Hat {} value: {}".format(i, str(hat)) )

    # ALL CODE TO DRAW SHOULD GO ABOVE THIS COMMENT
    # Limit to 20 frames per second
    clock.tick(20)

# Close the window and quit.
# If you forget this line, the program will 'hang'
# on exit if running from IDLE.
pygame.quit()

