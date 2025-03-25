#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <switch.h>
#include <stdio.h>

// Nintendo Switch screen size
#define SCREEN_WIDTH 1280
#define SCREEN_HEIGHT 720

// Colors
#define BACKGROUND_COLOR 30, 30, 30, 255  // Dark background
#define ITEM_COLOR 60, 60, 60, 255        // Menu items color (dark gray)
#define SELECTED_COLOR 100, 150, 255, 255 // Selected item (light blue)
#define TEXT_COLOR 255, 255, 255, 255     // Text color (white)

// Button design
#define BUTTON_WIDTH 500 // Button width
#define BUTTON_HEIGHT 80 // Button height

// Structure for menu items
typedef struct
{
    SDL_Rect rect;    // Position and size of the item
    bool selected;    // Selection state
    const char *text; // Text to display on the button
} MenuItem;

// Function to check if a point is inside a rectangle
bool pointInRect(int x, int y, SDL_Rect rect)
{
    return (x >= rect.x && x <= rect.x + rect.w &&
            y >= rect.y && y <= rect.y + rect.h);
}

// Variable to control the main loop lifecycle
bool running = true;

// Function to execute the actions
void doAction(int actionIndex)
{
    switch (actionIndex)
    {
    case 0:
        // Do reboot
        spsmInitialize();
        spsmShutdown(true);
        break;
    case 1:
        // Do power off
        spsmInitialize();
        spsmShutdown(false);
        break;
    case 2:
        // Exit application
        running = false;
        break;
    }
}

// Create menu items
MenuItem menuItems[3] = {
    {{(SCREEN_WIDTH - BUTTON_WIDTH) / 2, 200, BUTTON_WIDTH, BUTTON_HEIGHT}, false, "Reboot"},    // Item 1
    {{(SCREEN_WIDTH - BUTTON_WIDTH) / 2, 320, BUTTON_WIDTH, BUTTON_HEIGHT}, false, "Power off"}, // Item 2
    {{(SCREEN_WIDTH - BUTTON_WIDTH) / 2, 440, BUTTON_WIDTH, BUTTON_HEIGHT}, false, "Cancel"}     // Item 3
};

// Index of the selected item
int selectedButtonIndex = 0;

// Function to select a button
void setSelectedButton(int selectedIndex)
{
    // Deselect previously selected item
    menuItems[selectedButtonIndex].selected = false;

    // Select new item
    selectedButtonIndex = selectedIndex;
    menuItems[selectedButtonIndex].selected = true;
}

int main(int argc, char **argv)
{
    // Initialise sockets
    socketInitializeDefault();

    // Redirect stdout & stderr over network to nxlink
    nxlinkStdio();

    printf("Application started.\n");

    // Initialize SDL
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_JOYSTICK) < 0)
    {
        printf("Error initializing SDL: %s\n", SDL_GetError());
        return 1;
    }
    printf("SDL initialized successfully.\n");

    // Initialize SDL_ttf
    if (TTF_Init() < 0)
    {
        printf("Error initializing SDL_ttf: %s\n", TTF_GetError());
        SDL_Quit();
        return 1;
    }
    printf("SDL_ttf initialized successfully.\n");

    Result rc = romfsInit();
    if (R_FAILED(rc))
    {
        printf("Error initializing romfs: %08X\n", rc);
        return 1;
    }
    printf("romfs initialized successfully.\n");

    // Load a font
    TTF_Font *font = TTF_OpenFont("romfs:/OpenSans-Light.ttf", 36);
    if (!font)
    {
        printf("Error loading font: %s\n", TTF_GetError());
        TTF_Quit();
        SDL_Quit();
        return 1;
    }
    printf("Font loaded successfully.\n");

    // Initialize joystick (controller)
    if (SDL_NumJoysticks() < 1)
    {
        printf("No controller connected.\n");
    }
    else
    {
        SDL_Joystick *joystick = SDL_JoystickOpen(0);
        if (!joystick)
        {
            printf("Error opening controller: %s\n", SDL_GetError());
        }
        else
        {
            printf("Controller connected: %s\n", SDL_JoystickName(joystick));
        }
    }

    // Create a window and a renderer
    SDL_Window *window = SDL_CreateWindow("Vertical Menu", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, SCREEN_WIDTH, SCREEN_HEIGHT, 0);
    if (!window)
    {
        printf("Error creating window: %s\n", SDL_GetError());
        TTF_CloseFont(font);
        TTF_Quit();
        SDL_Quit();
        return 1;
    }
    printf("Window created successfully.\n");

    SDL_Renderer *renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!renderer)
    {
        printf("Error creating renderer: %s\n", SDL_GetError());
        SDL_DestroyWindow(window);
        TTF_CloseFont(font);
        TTF_Quit();
        SDL_Quit();
        return 1;
    }
    printf("Renderer created successfully.\n");

    // Enable touch events
    SDL_SetHint(SDL_HINT_TOUCH_MOUSE_EVENTS, "1");

    // Select the first button by default
    setSelectedButton(0);
    printf("Button %d selected by default.\n", selectedButtonIndex + 1);

    // Variables for analog control
    const int ANALOG_THRESHOLD = 8000; // Analog sensitivity
    int lastDirection = 0;             // Last detected direction (0 = none, 1 = up, 2 = down)

    // Main loop
    while (running)
    {
        // Process events
        SDL_Event event;
        while (SDL_PollEvent(&event))
        {
            if (event.type == SDL_QUIT)
            {
                printf("SDL_QUIT event received. Exiting application...\n");
                running = false;
            }

            // Handle touch events
            if (event.type == SDL_FINGERDOWN || event.type == SDL_MOUSEBUTTONDOWN)
            {
                int touchX, touchY;

                if (event.type == SDL_FINGERDOWN)
                {
                    // Convert normalized touch coordinates to screen coordinates
                    touchX = event.tfinger.x * SCREEN_WIDTH;
                    touchY = event.tfinger.y * SCREEN_HEIGHT;
                }
                else // SDL_MOUSEBUTTONDOWN (for testing in non-touch environments)
                {
                    touchX = event.button.x;
                    touchY = event.button.y;
                }

                printf("Touch detected at (%d, %d)\n", touchX, touchY);

                // Check which button was touched
                for (int i = 0; i < 3; i++)
                {
                    if (pointInRect(touchX, touchY, menuItems[i].rect))
                    {
                        // If this is a finger down event, select the button and trigger the action
                        if (event.type == SDL_FINGERDOWN)
                        {
                            printf("Button %d touched\n", i + 1);

                            // Change selected button
                            setSelectedButton(i);

                            // Add delay before action to prevent app crash
                            Uint32 startTime = SDL_GetTicks();
                            while (SDL_GetTicks() - startTime < 100)
                            {
                                SDL_PumpEvents();
                                SDL_Delay(10);
                            }

                            printf("Button %d activated by touch\n", selectedButtonIndex + 1);
                            doAction(selectedButtonIndex);
                        }

                        break;
                    }
                }
            }

            // Check controller input (d-pad, analog, and A/B buttons)
            if (event.type == SDL_JOYBUTTONDOWN)
            {
                // Check if the "A" button was pressed
                if (event.jbutton.button == 0)
                {
                    printf("A pressed. Selected item: %d\n", selectedButtonIndex + 1);
                    doAction(selectedButtonIndex);
                }

                // Check if the "B" button was pressed
                if (event.jbutton.button == 1)
                {
                    printf("B pressed. Exiting application...\n");
                    running = false;
                }

                // Check if the d-pad "up" button was pressed
                if (event.jbutton.button == 13)
                {
                    printf("UP pressed. ");

                    // Move up
                    if (selectedButtonIndex > 0)
                    {
                        // Change selected button
                        setSelectedButton(selectedButtonIndex - 1);
                        printf("Button %d selected.\n", selectedButtonIndex + 1);
                    }
                }

                // Check if the d-pad "down" button was pressed
                if (event.jbutton.button == 15)
                {
                    printf("DOWN pressed. ");

                    // Move down
                    if (selectedButtonIndex < 2)
                    {
                        // Change selected button
                        setSelectedButton(selectedButtonIndex + 1);
                        printf("Button %d selected.\n", selectedButtonIndex + 1);
                    }
                }
            }

            // Check analog sticks to select the item
            if (event.type == SDL_JOYAXISMOTION)
            {
                // Left analog stick (axis 1) or right analog stick (axis 3)
                if (event.jaxis.axis == 1 || event.jaxis.axis == 3)
                {
                    int yAxisValue = event.jaxis.value;

                    if (yAxisValue < -ANALOG_THRESHOLD && lastDirection != 1)
                    {
                        // Analog up
                        if (selectedButtonIndex > 0)
                        {
                            // Change selected button
                            setSelectedButton(selectedButtonIndex - 1);
                            printf("Button %d selected.\n", selectedButtonIndex + 1);
                        }
                        lastDirection = 1; // Update last direction
                    }
                    else if (yAxisValue > ANALOG_THRESHOLD && lastDirection != 2)
                    {
                        // Analog down
                        if (selectedButtonIndex < 2)
                        {
                            // Change selected button
                            setSelectedButton(selectedButtonIndex + 1);
                            printf("Button %d selected.\n", selectedButtonIndex + 1);
                        }
                        lastDirection = 2; // Update last direction
                    }
                    else if (yAxisValue > -ANALOG_THRESHOLD && yAxisValue < ANALOG_THRESHOLD)
                    {
                        lastDirection = 0; // Analog neutral
                    }
                }
            }
        }

        // Clear the screen
        SDL_SetRenderDrawColor(renderer, BACKGROUND_COLOR);
        SDL_RenderClear(renderer);

        // Draw menu items
        for (int i = 0; i < 3; i++)
        {
            // Draw the button rectangle
            SDL_Color color = (menuItems[i].selected) ? (SDL_Color){SELECTED_COLOR} : (SDL_Color){ITEM_COLOR};
            SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
            SDL_RenderFillRect(renderer, &menuItems[i].rect);

            // Render the text
            SDL_Color textColor = {TEXT_COLOR};
            SDL_Surface *textSurface = TTF_RenderText_Blended(font, menuItems[i].text, textColor);
            if (!textSurface)
            {
                printf("Error rendering text: %s\n", TTF_GetError());
                continue;
            }

            SDL_Texture *textTexture = SDL_CreateTextureFromSurface(renderer, textSurface);
            if (!textTexture)
            {
                printf("Error creating texture: %s\n", SDL_GetError());
                SDL_FreeSurface(textSurface);
                continue;
            }

            // Center the text in the button
            int textWidth = textSurface->w;
            int textHeight = textSurface->h;
            SDL_Rect textRect = {
                menuItems[i].rect.x + (menuItems[i].rect.w - textWidth) / 2,
                menuItems[i].rect.y + (menuItems[i].rect.h - textHeight) / 2,
                textWidth,
                textHeight};

            SDL_RenderCopy(renderer, textTexture, NULL, &textRect);

            // Cleanup
            SDL_FreeSurface(textSurface);
            SDL_DestroyTexture(textTexture);
        }

        // Update the screen
        SDL_RenderPresent(renderer);
    }

    // Cleanup
    romfsExit();
    TTF_CloseFont(font);
    TTF_Quit();
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    
    printf("Application ended.\n");
    socketExit();
    return 0;
}