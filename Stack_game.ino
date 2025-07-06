#include <MD_MAX72xx.h>
#include <SPI.h>

// Define MAX7219 settings
#define MAX_DEVICES 4  // 4-in-1 Display
#define CLK_PIN 11
#define DATA_PIN 12
#define CS_PIN 10

MD_MAX72XX matrix = MD_MAX72XX(MD_MAX72XX::FC16_HW, DATA_PIN, CLK_PIN, CS_PIN, MAX_DEVICES);

// Button Pin
#define BUTTON_PIN 2  // Push button pin (using internal pull-up)

// Grid size
#define WIDTH 8    // 8 columns (single 8x8 display width)
#define HEIGHT 32  // 32 rows (4 modules x 8 rows)

// Game variables
int blockPos = 0;              // Current block x-position (leftmost LED, 0-4)
const int BLOCK_WIDTH = 4;     // Block width in LEDs
int blockRow = 0;              // Current block row (0 = bottom)
bool blockMoving = true;       // Is block moving?
int stack[HEIGHT][WIDTH];      // Stack state (1 = LED on, 0 = off)
int stackHeight = 0;           // Highest occupied row
bool gameOver = false;         // Game over flag
unsigned long lastMove = 0;    // Last block move time
int moveDelay = 200;           // Initial move delay (ms)
bool moveRight = true;         // Block movement direction
bool testMode = false;         // Test mode for display mapping
int testRow = 0;               // Current row for test mode
const bool ROTATE_DISPLAY = true; // Adjust for FC-16 rotation (rows/columns swapped)

void setup() {
  Serial.begin(9600);
  Serial.println("Setup started");

  matrix.begin(); // Initialize display
  matrix.clear();

  pinMode(BUTTON_PIN, INPUT_PULLUP); // Button with internal pull-up
  randomSeed(analogRead(A0));        // Random seed using unused analog pin
  resetGame();
  Serial.println("Game initialized. Send 't' in Serial Monitor to enter test mode.");
}

void resetGame() {
  // Clear stack array
  for (int row = 0; row < HEIGHT; row++) {
    for (int col = 0; col < WIDTH; col++) {
      stack[row][col] = 0;
    }
  }
  blockPos = random(0, WIDTH - BLOCK_WIDTH + 1); // Random start (0-4)
  blockRow = 0;
  stackHeight = 0;
  gameOver = false;
  blockMoving = true;
  moveDelay = 200;
  moveRight = true;
  testMode = false;
  testRow = 0;
  updateDisplay();
  Serial.println("Game reset");
}

void updateDisplay() {
  matrix.clear();

  if (testMode) {
    // Test mode: Light up one row at a time
    for (int col = 0; col < WIDTH; col++) {
      if (ROTATE_DISPLAY) {
        matrix.setPoint(col, testRow, true); // Rotate 90° for vertical display
      } else {
        matrix.setPoint(testRow, col, true);
      }
    }
    Serial.print("Test mode: Lighting row ");
    Serial.println(testRow);
    return;
  }

  // Draw stack
  for (int row = 0; row < HEIGHT; row++) {
    for (int col = 0; col < WIDTH; col++) {
      if (stack[row][col]) {
        if (ROTATE_DISPLAY) {
          matrix.setPoint(col, row, true); // Rotate 90°
        } else {
          matrix.setPoint(row, col, true);
        }
      }
    }
  }

  // Draw moving block if game not over
  if (!gameOver && blockMoving) {
    for (int col = blockPos; col < blockPos + BLOCK_WIDTH; col++) {
      if (col >= 0 && col < WIDTH) {
        if (ROTATE_DISPLAY) {
          matrix.setPoint(col, blockRow, true); // Rotate 90°
        } else {
          matrix.setPoint(blockRow, col, true);
        }
      }
    }
    Serial.print("Block at row ");
    Serial.print(blockRow);
    Serial.print(", cols ");
    Serial.print(blockPos);
    Serial.print("-");
    Serial.println(blockPos + BLOCK_WIDTH - 1);
  }
}

void dropBlock() {
  blockMoving = false; // Stop block movement
  bool placed = false; // Did block land on stack?

  // Check if block lands on stack or bottom
  if (blockRow == 0 || stackHeight > 0) {
    for (int col = blockPos; col < blockPos + BLOCK_WIDTH; col++) {
      if (col >= 0 && col < WIDTH) {
        // If row 0 or overlaps with stack below, place block
        if (blockRow == 0 || stack[blockRow - 1][col] == 1) {
          stack[blockRow][col] = 1;
          placed = true;
        }
      }
    }
  }

  // If no LEDs placed (missed stack), game over
  if (!placed && blockRow > 0) {
    gameOver = true;
    Serial.println("Game over: Missed stack");
    return;
  }

  // Update stack height
  if (blockRow >= stackHeight) {
    stackHeight = blockRow + 1;
  }

  // Check if stack reached top
  if (stackHeight >= HEIGHT) {
    gameOver = true;
    Serial.println("Game over: Stack reached top");
    return;
  }

  // Move to next row
  blockRow++;
  blockPos = random(0, WIDTH - BLOCK_WIDTH + 1);
  blockMoving = true;

  // Speed up game
  moveDelay = max(50, moveDelay - 10); // Minimum delay 50ms
  Serial.print("Dropped block at row ");
  Serial.print(blockRow - 1);
  Serial.print(", new block at row ");
  Serial.println(blockRow);
  Serial.println("Stack after drop:");
  for (int col = 0; col < WIDTH; col++) {
    Serial.print(stack[blockRow - 1][col]);
  }
  Serial.println();
}

void loop() {
  // Check for Serial input to toggle test mode
  if (Serial.available()) {
    char c = Serial.read();
    if (c == 't') {
      testMode = !testMode;
      if (testMode) {
        testRow = 0;
        Serial.println("Entered test mode. Send 'n' or press button to advance row, 't' to exit.");
      } else {
        Serial.println("Exited test mode");
        resetGame();
      }
    } else if (testMode && c == 'n') {
      testRow = (testRow + 1) % HEIGHT;
      Serial.print("Test mode: Advanced to row ");
      Serial.println(testRow);
    }
    updateDisplay();
  }

  if (testMode) {
    // In test mode, advance row with button press
    if (digitalRead(BUTTON_PIN) == LOW) {
      delay(50); // Debounce
      if (digitalRead(BUTTON_PIN) == LOW) {
        testRow = (testRow + 1) % HEIGHT;
        Serial.print("Test mode: Advanced to row "); // Fixed typo: rxint -> print
        Serial.println(testRow);
        updateDisplay();
        while (digitalRead(BUTTON_PIN) == LOW); // Wait for release
      }
    }
    return;
  }

  if (gameOver) {
    // Flash display and wait for button to reset
    matrix.clear();
    delay(500);
    updateDisplay();
    delay(500);
    if (digitalRead(BUTTON_PIN) == LOW) {
      delay(50); // Debounce
      if (digitalRead(BUTTON_PIN) == LOW) {
        resetGame();
        Serial.println("Game restarted");
        while (digitalRead(BUTTON_PIN) == LOW); // Wait for release
      }
    }
    return;
  }

  // Move block left/right
  if (blockMoving && millis() - lastMove >= moveDelay) {
    blockPos += moveRight ? 1 : -1;
    if (blockPos <= 0) {
      blockPos = 0;
      moveRight = true;
    } else if (blockPos >= WIDTH - BLOCK_WIDTH) {
      blockPos = WIDTH - BLOCK_WIDTH;
      moveRight = false;
    }
    lastMove = millis();
    updateDisplay();
  }

  // Check button press to drop block
  if (digitalRead(BUTTON_PIN) == LOW) {
    delay(50); // Debounce
    if (digitalRead(BUTTON_PIN) == LOW) {
      Serial.println("Button pressed");
      dropBlock();
      updateDisplay();
      while (digitalRead(BUTTON_PIN) == LOW); // Wait for button release
    }
  }
}