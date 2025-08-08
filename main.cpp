#include <GL/glut.h>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <vector>
#include <algorithm>
#include <string>
#include <windows.h>




struct Platform {
    float x, y;                // 0, 1
    float width;              // 2
    bool breakable = false;   // 3
    bool broken = false;      // 4
    int standTime = 0;        // 5
    bool isSpike = false;     // 6
    bool isTarget = false;    // 7
    int respawnTime = 0;      // 8
    float shakeOffset = 0;    // 9
    bool shakeDir = true;     // 10

    // Moving platform additions
    bool isMoving = false;      // 11
    bool moveHorizontal = true; // 12
    float moveRange = 0.0f;     // 13 ✅ float
    float moveSpeed = 0.0f;     // 14 ✅ float
    float originalX = 0.0f;     // 15 ✅ float
    float originalY = 0.0f;     // 16 ✅ float
    bool moveDir = true;        // 17
};


float ballX = 0, ballY = -0.5f, previousBallY = -0.5f;
float velocityY = 0, gravity = -0.0017f;
float radius = 0.07f;
bool onGround = true;
bool moveLeft = false, moveRight = false;
float rotationAngle = 0.0f;
bool gameOver = false;
bool isRunning = true;
bool updateLoopRunning = false;


enum GameState {
    MENU,
    PLAYING,
    GAME_OVER
};

GameState currentState = MENU;

GLuint ballTexture, platformStartTexture, platformTileTexture, platformEndTexture;
GLuint breakablePlatformTexture, spikeTexture, backgroundTexture,targetTexture;


std::vector<Platform> platforms = {};


GLuint loadBMP(const char* filename) {
    FILE* file = fopen(filename, "rb");
    if (!file) {
        printf("Image not found: %s\n", filename);
        return 0;
    }

    unsigned char header[54];
    fread(header, sizeof(unsigned char), 54, file);
    int width = *(int*)&header[18];
    int height = *(int*)&header[22];

    int imageSize = 3 * width * height;
    unsigned char* data = new unsigned char[imageSize];
    fread(data, sizeof(unsigned char), imageSize, file);
    fclose(file);

    for (int i = 0; i < imageSize; i += 3)
        std::swap(data[i], data[i + 2]);

    GLuint textureID;
    glGenTextures(1, &textureID);
    glBindTexture(GL_TEXTURE_2D, textureID);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, data);

    delete[] data;
    return textureID;
}

void loadLevel(const char* filename) {
    platforms.clear();

    FILE* file = fopen(filename, "r");
    if (!file) {
        printf("Failed to open level file: %s\n", filename);
        exit(1);
    }

    float x, y, width, moveRange, moveSpeed;
    int breakable, isSpike, isTarget, isMoving, moveHorizontal;

    while (!feof(file)) {
        char line[256];
        if (!fgets(line, sizeof(line), file)) break;
        if (line[0] == '#' || line[0] == '\n') continue;

        if (sscanf(line, "%f %f %f %d %d %d %d %d %f %f",
                   &x, &y, &width, &breakable, &isSpike, &isTarget,
                   &isMoving, &moveHorizontal, &moveRange, &moveSpeed) == 10) {

            Platform p;
            p.x = x;
            p.y = y;
            p.width = width;
            p.breakable = breakable;
            p.isSpike = isSpike;
            p.isTarget = isTarget;
            p.isMoving = isMoving;
            p.moveHorizontal = moveHorizontal;
            p.moveRange = moveRange;
            p.moveSpeed = moveSpeed;

            if (p.isMoving) {
                p.originalX = p.x;
                p.originalY = p.y;
            }

            platforms.push_back(p);
        }
    }

    fclose(file);
}

void drawPlatformTexture(GLuint textureID, float x, float top, float bottom, float width) {
    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, textureID);
    glBegin(GL_QUADS);
    glTexCoord2f(0, 1); glVertex2f(x, top);
    glTexCoord2f(1, 1); glVertex2f(x + width, top);
    glTexCoord2f(1, 0); glVertex2f(x + width, bottom);
    glTexCoord2f(0, 0); glVertex2f(x, bottom);
    glEnd();
}

void drawPlatforms() {
    for (Platform& p : platforms) {
        if (p.breakable && p.broken) continue;

        if (p.isTarget) {
            drawPlatformTexture(targetTexture, p.x - p.width / 2, p.y, p.y - 0.1f, p.width);
            continue; // skip regular drawing
        }




        float height = 0.08f;
        float left = p.x - p.width / 2 + (p.breakable ? p.shakeOffset : 0);
        float top = p.y;
        float bottom = p.y - height;

        glEnable(GL_TEXTURE_2D);

        if (p.isSpike || p.breakable) {
            GLuint tex = p.isSpike ? spikeTexture : breakablePlatformTexture;
            float segmentWidth = 0.1f;
            float x = left;
            int count = std::max(1, (int)(p.width / segmentWidth));
            for (int i = 0; i < count; i++) {
                drawPlatformTexture(tex, x, top, bottom, segmentWidth);
                x += segmentWidth;
            }
        } else {
            float segmentWidth = 0.1f;
            float x = left;
            drawPlatformTexture(platformStartTexture, x, top, bottom, segmentWidth);
            x += segmentWidth;
            int tileCount = std::max(0, (int)((p.width - segmentWidth * 2) / segmentWidth));
            for (int i = 0; i < tileCount; i++) {
                drawPlatformTexture(platformTileTexture, x, top, bottom, segmentWidth);
                x += segmentWidth;
            }
            drawPlatformTexture(platformEndTexture, x, top, bottom, segmentWidth);
        }

        glDisable(GL_TEXTURE_2D);
    }
}

void drawBall() {
    if (gameOver) return;
    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, ballTexture);
    glColor3f(1, 1, 1);

    glPushMatrix();
    glTranslatef(ballX, ballY, 0);
    glRotatef(rotationAngle, 0, 0, 1);

    glBegin(GL_POLYGON);
    for (int i = 0; i <= 360; i++) {
        float angle = i * 3.1416f / 180.0f;
        float x = radius * cos(angle);
        float y = radius * sin(angle);
        float u = 0.5f + 0.5f * cos(angle);
        float v = 0.5f + 0.5f * sin(angle);
        glTexCoord2f(u, v);
        glVertex2f(x, y);
    }
    glEnd();

    glPopMatrix();
    glDisable(GL_TEXTURE_2D);
}
void update(int value) {
    // ⏲ Schedule next update at ~60 FPS (16ms)
    glutTimerFunc(16, update, 0);

    // === Still animate moving platforms ===
    for (Platform& p : platforms) {
        if (!p.isMoving || p.broken) continue;

        float& pos = p.moveHorizontal ? p.x : p.y;
        float& origin = p.moveHorizontal ? p.originalX : p.originalY;

        if (p.moveDir)
            pos += p.moveSpeed;
        else
            pos -= p.moveSpeed;

        if (fabs(pos - origin) >= p.moveRange)
            p.moveDir = !p.moveDir;
    }

    // === Only update game logic if playing ===
    if (!isRunning || gameOver || currentState != PLAYING) {
        glutPostRedisplay();
        return;
    }

    previousBallY = ballY;

    // Horizontal movement
    if (moveLeft && ballX - radius > -2.0f) {
        ballX -= 0.01f;
        rotationAngle += 6.0f;
    }
    if (moveRight && ballX + radius < 2.0f) {
        ballX += 0.01f;
        rotationAngle -= 6.0f;
    }

    // Apply gravity
    velocityY += gravity;
    ballY += velocityY;
    onGround = false;

    for (Platform& p : platforms) {
        if (p.broken) {
            if (p.breakable) {
                p.respawnTime++;
                if (p.respawnTime >= 150) {
                    p.broken = false;
                    p.respawnTime = 0;
                    p.standTime = 0;
                    p.shakeOffset = 0;
                }
            }
            continue;
        }

        float height = 0.08f;
        float left = p.x - p.width / 2;
        float right = p.x + p.width / 2;
        float top = p.y;
        float bottom = p.y - height;

        // Spike full contact
        if (p.isSpike) {
            bool xOverlap = (ballX + radius > left) && (ballX - radius < right);
            bool yOverlap = (ballY + radius > bottom) && (ballY - radius < top);
            if (xOverlap && yOverlap) {
                gameOver = true;
                isRunning = false;
                return;
            }
        }

        // Target platform
        if (p.isTarget) {
            bool xOverlap = (ballX + radius > left) && (ballX - radius < right);
            bool yOverlap = (ballY + radius > bottom) && (ballY - radius < top);
            if (xOverlap && yOverlap) {
                printf("🎉 YOU WIN! 🎉\n");
                gameOver = true;
                isRunning = false;
                return;
            }
        }

        // Top collision
        bool withinX = ballX + radius > left && ballX - radius < right;
        bool crossed = previousBallY - radius >= top && ballY - radius <= top && velocityY <= 0;

        if (withinX && crossed) {
            ballY = top + radius;

            if (velocityY < -0.03f) {
                velocityY = -velocityY * 0.5f;
                onGround = false;
            } else {
                velocityY = 0;
                onGround = true;
            }

            if (p.breakable) {
                p.standTime++;
                if (p.standTime > 50) {
                    p.broken = true;
                    p.respawnTime = 0;
                    velocityY = 0.03f;
                    onGround = false;
                } else if (p.standTime > 20) {
                    p.shakeOffset += (p.shakeDir ? 0.006f : -0.006f);
                    p.shakeDir = !p.shakeDir;
                }
            }

            break;
        } else {
            if (p.breakable) {
                p.standTime = 0;
                p.shakeOffset = 0;
            }
        }
    }

    glutPostRedisplay();
}



void display() {
    glClear(GL_COLOR_BUFFER_BIT);

    if (currentState == MENU) {
        glEnable(GL_TEXTURE_2D);
        glBindTexture(GL_TEXTURE_2D, backgroundTexture);
        glColor3f(1, 1, 1);
        glBegin(GL_QUADS);
        glTexCoord2f(0, 1); glVertex2f(-2.0f, 1.2f);
        glTexCoord2f(1, 1); glVertex2f(2.0f, 1.2f);
        glTexCoord2f(1, 0); glVertex2f(2.0f, -1.2f);
        glTexCoord2f(0, 0); glVertex2f(-2.0f, -1.2f);
        glEnd();
        glDisable(GL_TEXTURE_2D);

        glColor3f(0.2f, 0.2f, 0.7f);

        glRasterPos2f(-0.4f, 0.2f);
        const char* title = "2D Bounce Ball Game";
        for (int i = 0; title[i] != '\0'; i++)
            glutBitmapCharacter(GLUT_BITMAP_HELVETICA_18, title[i]);

        glRasterPos2f(-0.35f, 0.0f);
        const char* start = "Press ENTER to Start";
        for (int i = 0; start[i] != '\0'; i++)
            glutBitmapCharacter(GLUT_BITMAP_HELVETICA_12, start[i]);
        glRasterPos2f(-0.4f, -0.3f);
        const char* levelInfo = "Press 1 or 2 to Load Levels";
        for (int i = 0; levelInfo[i] != '\0'; i++)
        glutBitmapCharacter(GLUT_BITMAP_HELVETICA_12, levelInfo[i]);

        glRasterPos2f(-0.3f, -0.2f);
        const char* quit = "Press ESC to Quit";
        for (int i = 0; quit[i] != '\0'; i++)
            glutBitmapCharacter(GLUT_BITMAP_HELVETICA_12, quit[i]);

        glutSwapBuffers();
        return;
    }

    // === In-Game drawing ===
    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, backgroundTexture);
    glColor3f(1, 1, 1);
    glBegin(GL_QUADS);
    glTexCoord2f(0, 1); glVertex2f(-2.0f, 1.2f);
    glTexCoord2f(1, 1); glVertex2f(2.0f, 1.2f);
    glTexCoord2f(1, 0); glVertex2f(2.0f, -1.2f);
    glTexCoord2f(0, 0); glVertex2f(-2.0f, -1.2f);
    glEnd();
    glDisable(GL_TEXTURE_2D);

    drawPlatforms();
    drawBall();

    if (gameOver) {
        glColor3f(1.0f, 0.0f, 0.0f);
        glRasterPos2f(-0.2f, 0.0f);
        const char* msg = "GAME OVER";
        for (int i = 0; msg[i] != '\0'; i++)
            glutBitmapCharacter(GLUT_BITMAP_HELVETICA_18, msg[i]);

        glRasterPos2f(-0.3f, -0.1f);
        const char* restart = "Press R to Restart";
        for (int i = 0; restart[i] != '\0'; i++)
            glutBitmapCharacter(GLUT_BITMAP_HELVETICA_12, restart[i]);

        glRasterPos2f(-0.3f, -0.2f);
        const char* back = "Press M to go to Menu";
        for (int i = 0; back[i] != '\0'; i++)
            glutBitmapCharacter(GLUT_BITMAP_HELVETICA_12, back[i]);
    }

    glutSwapBuffers();
}

void keyboardDown(unsigned char key, int x, int y) {
    if (currentState == MENU) {
        if (key == 13) { // ENTER key
            currentState = PLAYING;

        }
        if (key == 27) { // ESC key
            exit(0);
        }
        if (key == '1') {
            loadLevel("level/level1.txt");
            ballX = 0; ballY = -0.5f;
            velocityY = 0;
            gameOver = false;
            currentState = PLAYING;
            isRunning = true;

        }

        if (key == '2') {
            loadLevel("level/level2.txt");
            ballX = 0; ballY = -0.5f;
            velocityY = 0;
            gameOver = false;
            currentState = PLAYING;
            isRunning = true;

        }
        return;
    }




    // === In-Game Controls ===
    if (key == 'a') moveLeft = true;
    if (key == 'd') moveRight = true;
    if ((key == 'w' || key == ' ') && onGround && !gameOver) {
        velocityY = 0.043f;
        onGround = false;
    }
    if (key == 'r') {
        ballX = 0;
        ballY = -0.5f;
        velocityY = 0;
        gameOver = false;
        isRunning = true;

        for (Platform& p : platforms) {
            p.broken = false;
            p.respawnTime = 0;
            p.standTime = 0;
            p.shakeOffset = 0;

            if (p.isMoving) {
                p.x = p.originalX;
                p.y = p.originalY;
                p.moveDir = true;
            }
        }

    // ❌ DO NOT call update(0) again here!
    }
    if (key == 'm') {
        currentState = MENU;
        isRunning = false;
        gameOver = false;
        ballX = 0;
        ballY = -0.5f;
        velocityY = 0;

        for (Platform& p : platforms) {
            p.broken = false;
            p.respawnTime = 0;
            p.standTime = 0;
            p.shakeOffset = 0;
        }
        if (!isRunning) {
            isRunning = true;

        }
        glutPostRedisplay();  // Refresh screen
    }
}

void keyboardUp(unsigned char key, int x, int y) {
    if (key == 'a') moveLeft = false;
    if (key == 'd') moveRight = false;
}

GLuint loadTexture(const char* filename) {
    GLuint textureID;
    int width, height;
    unsigned char header[54];
    unsigned char* data;
    FILE* file = fopen(filename, "rb");
    if (!file) {
        printf("Image %s not found!\n", filename);
        exit(1);
    }

    fread(header, 1, 54, file); // skip header
    width = *(int*)&header[18];
    height = *(int*)&header[22];

    int size = 3 * width * height;
    data = new unsigned char[size];
    fread(data, 1, size, file);
    fclose(file);

    for (int i = 0; i < size; i += 3) {
        std::swap(data[i], data[i + 2]); // BGR to RGB
    }

    glGenTextures(1, &textureID);
    glBindTexture(GL_TEXTURE_2D, textureID);

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height,
                 0, GL_RGB, GL_UNSIGNED_BYTE, data);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    delete[] data;
    return textureID;
}

void init() {

    glEnable(GL_TEXTURE_2D);
    glClearColor(0.95f, 0.95f, 1.0f, 1.0f);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluOrtho2D(-2.0, 2.0, -1.2, 1.2);

    loadLevel("level/level1.txt");

    ballTexture = loadBMP("image/ball.bmp");
    platformStartTexture = loadBMP("image/platform_start.bmp");
    platformTileTexture  = loadBMP("image/platform_tile.bmp");
    platformEndTexture   = loadBMP("image/platform_end.bmp");
    backgroundTexture    = loadBMP("image/background.bmp");
    breakablePlatformTexture = loadBMP("image/platform_breakable.bmp");
    spikeTexture = loadBMP("image/spike.bmp");
    targetTexture = loadTexture("image/target.bmp");
    for (Platform& p : platforms) {
        if (p.isMoving) {
            p.originalX = p.x;
            p.originalY = p.y;
        }
    }
}

int main(int argc, char** argv) {
    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB);
    glutInitWindowSize(900, 700);
    glutCreateWindow("2D Bounce Ball game");

    HWND hwnd = FindWindowA(NULL, "2D Bounce Ball game");
    HICON hIcon = (HICON)LoadImageA(NULL, "ball.ico", IMAGE_ICON, 32, 32, LR_LOADFROMFILE);
    if (hIcon != NULL && hwnd != NULL) {
        SendMessage(hwnd, WM_SETICON, ICON_SMALL, (LPARAM)hIcon);
        SendMessage(hwnd, WM_SETICON, ICON_BIG, (LPARAM)hIcon);
    }

    init();
    glutDisplayFunc(display);
    glutKeyboardFunc(keyboardDown);
    glutKeyboardUpFunc(keyboardUp);
    glutTimerFunc(0, update, 0);

    glutMainLoop();
    return 0;
}
