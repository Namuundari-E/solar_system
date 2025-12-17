/*
 * Enhanced Texture-Mapped Solar System with Interactive Features
 * Author: Э.Намуундарь (Enhanced Version)
 *
 * Compilation:
 * g++ -o solar_system solar_system.cpp -lglut -lGLU -lGL -lm
 *
 * New Features:
 * - Planet axis rotation
 * - Hover tooltips with planet details
 * - Click to focus on planets with smooth camera animation
 * - Wikipedia links and gravity simulation when focused
 * - Planet-specific time systems
 *
 * Controls:
 * - Mouse drag: Rotate view
 * - Mouse wheel: Zoom in/out
 * - Mouse hover: Show planet info
 * - Left click: Focus on planet
 * - 'o': Toggle orbits
 * - '+/-': Increase/decrease animation speed
 * - 'r': Reset view / Unfocus planet
 * - 'w': Open Wikipedia page (when planet focused)
 * - 'g': Toggle gravity simulation (when planet focused)
 * - ESC: Exit
 */

#include <GL/glut.h>
#include <GL/glu.h>
#include <cmath>
#include <iostream>
#include <vector>
#include <string>
#include <sstream>
#include <iomanip>
#include <cstdlib>

// STB Image - single header image loading library
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// Window dimensions
int windowWidth = 1400;
int windowHeight = 900;

// Camera parameters
float cameraDistance = 250.0f;
float cameraAngleX = 30.0f;
float cameraAngleY = 45.0f;
float cameraZoom = 1.0f;

// Camera animation for focus
bool isCameraAnimating = false;
float targetCameraDistance = 250.0f;
float targetCameraAngleX = 30.0f;
float targetCameraAngleY = 45.0f;
float targetCameraZoom = 1.0f;
float animationProgress = 0.0f;
float startCameraDistance, startCameraAngleX, startCameraAngleY, startCameraZoom;

// Mouse control
int lastMouseX = 0;
int lastMouseY = 0;
bool isMouseDragging = false;
int mouseX = 0;
int mouseY = 0;

// Animation
float animationSpeed = 1.0f;
float time_elapsed = 0.0f;
bool showOrbits = true;

// Focus system
int focusedPlanetIndex = -1;
float focusTargetX = 0.0f, focusTargetY = 0.0f, focusTargetZ = 0.0f;

// Hover system
int hoveredPlanetIndex = -1;

// Gravity simulation
bool showGravitySimulation = false;
float gravityBallY = 0.0f;
float gravityBallVelocity = 0.0f;
float gravitySimTime = 0.0f;

// Moon structure
struct Moon {
    const char* name;
    float radius;
    float orbitRadius;
    float orbitSpeed;
    float angle;
    float color[3];
    GLuint textureID;
};

// Planet structure with enhanced data
struct Planet {
    const char* name;
    float radius;
    float orbitRadius;
    float orbitSpeed;
    float rotationSpeed;
    float angle;
    float axisRotation; // New: axis rotation angle
    float tilt;
    GLuint textureID;
    float color[3];
    bool hasRings;
    float ringInnerRadius;
    float ringOuterRadius;
    float textureRotation;
    GLuint ringTextureID;
    std::vector<Moon> moons;

    // Enhanced planet data
    float dayLength; // Earth days for one rotation
    float yearLength; // Earth days for one orbit
    float gravity; // m/s^2
    const char* wikiUrl;
};

std::vector<Planet> planets;
Planet sun;

// Galaxy background
struct Star {
    float x, y, z;
    float brightness;
    float size;
};
std::vector<Star> galaxyStars;

// Function to open URL in default browser (cross-platform)
void openURL(const char* url) {
#ifdef _WIN32
    std::string command = "start " + std::string(url);
#elif __APPLE__
    std::string command = "open " + std::string(url);
#else
    std::string command = "xdg-open " + std::string(url);
#endif
    system(command.c_str());
}

// Calculate planet's local time
std::string getPlanetTimeString(const Planet& p) {
    if (p.dayLength == 0) return "N/A";

    float totalRotations = time_elapsed * animationSpeed / (p.dayLength * 0.1f);
    int days = (int)totalRotations;
    float hourFraction = (totalRotations - days) * 24.0f;
    int hours = (int)hourFraction;
    int minutes = (int)((hourFraction - hours) * 60.0f);

    std::ostringstream oss;
    oss << "Day " << days << ", " << std::setfill('0') << std::setw(2) << hours
        << ":" << std::setw(2) << minutes;
    return oss.str();
}

// Load texture from file using stb_image
GLuint loadTexture(const char* filename, bool hasAlpha = false) {
    stbi_set_flip_vertically_on_load(true);
    int width, height, channels;
    unsigned char* data = stbi_load(filename, &width, &height, &channels, 0);

    if (!data) {
        std::cerr << "Failed to load texture: " << filename << std::endl;
        std::cerr << "STB Error: " << stbi_failure_reason() << std::endl;
        return 0;
    }

    std::cout << "Loaded: " << filename << " (" << width << "x" << height << ", " << channels << " channels)" << std::endl;

    GLuint textureID;
    glGenTextures(1, &textureID);
    glBindTexture(GL_TEXTURE_2D, textureID);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    GLenum format;
    if (channels == 4) {
        format = GL_RGBA;
    } else if (channels == 3) {
        format = GL_RGB;
    } else if (channels == 1) {
        format = GL_LUMINANCE;
    } else {
        std::cerr << "Unsupported channel count: " << channels << std::endl;
        stbi_image_free(data);
        return 0;
    }

    glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format, GL_UNSIGNED_BYTE, data);
    gluBuild2DMipmaps(GL_TEXTURE_2D, format, width, height, format, GL_UNSIGNED_BYTE, data);

    stbi_image_free(data);
    return textureID;
}

// Create fallback procedural texture
GLuint createFallbackTexture(float r, float g, float b) {
    const int width = 256;
    const int height = 256;
    unsigned char* data = new unsigned char[width * height * 3];

    for (int i = 0; i < height; i++) {
        for (int j = 0; j < width; j++) {
            int idx = (i * width + j) * 3;
            float noise = ((rand() % 100) / 100.0f - 0.5f) * 0.2f;
            data[idx] = (unsigned char)(fmin(255, fmax(0, (r + noise) * 255)));
            data[idx + 1] = (unsigned char)(fmin(255, fmax(0, (g + noise) * 255)));
            data[idx + 2] = (unsigned char)(fmin(255, fmax(0, (b + noise) * 255)));
        }
    }

    GLuint textureID;
    glGenTextures(1, &textureID);
    glBindTexture(GL_TEXTURE_2D, textureID);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, data);

    delete[] data;
    return textureID;
}

// Initialize galaxy background
void initGalaxy() {
    galaxyStars.clear();

    for (int i = 0; i < 8000; i++) {
        Star s;

        float angle = ((rand() % 1000) / 1000.0f) * 2.0f * M_PI;
        float distance = pow((rand() % 1000) / 1000.0f, 0.6f) * 1200.0f;

        s.x = cos(angle) * distance + ((rand() % 1000) / 1000.0f - 0.5f) * 300.0f;
        s.y = ((rand() % 1000) / 1000.0f - 0.5f) * 200.0f;
        s.z = sin(angle) * distance + ((rand() % 1000) / 1000.0f - 0.5f) * 300.0f;

        s.brightness = 0.3f + (rand() % 1000) / 1000.0f * 0.7f;
        s.size = 1.0f + (rand() % 100) / 100.0f * 2.0f;

        galaxyStars.push_back(s);
    }
}

// Draw galaxy background
void drawGalaxy() {
    glDisable(GL_LIGHTING);
    glDisable(GL_TEXTURE_2D);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE);
    glDepthMask(GL_FALSE);

    glBegin(GL_POINTS);
    for (size_t i = 0; i < galaxyStars.size(); i++) {
        Star& s = galaxyStars[i];

        float colorVar = (rand() % 100) / 100.0f;
        if (colorVar < 0.6f) {
            glColor4f(1.0f, 1.0f, 1.0f, s.brightness);
        } else if (colorVar < 0.8f) {
            glColor4f(0.7f, 0.8f, 1.0f, s.brightness);
        } else {
            glColor4f(1.0f, 0.9f, 0.7f, s.brightness);
        }

        glVertex3f(s.x, s.y, s.z);
    }
    glEnd();

    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);
    glEnable(GL_LIGHTING);
}

// Initialize textures with enhanced planet data
void initTextures() {
    std::cout << "\n=== Loading Planet Textures ===" << std::endl;

    // Sun
    sun.name = "Sun";
    sun.radius = 20.0f;
    sun.orbitRadius = 0.0f;
    sun.orbitSpeed = 0.0f;
    sun.rotationSpeed = 0.1f;
    sun.angle = 0.0f;
    sun.axisRotation = 0.0f;
    sun.tilt = 0.0f;
    sun.hasRings = false;
    sun.textureRotation = 0.0f;
    sun.dayLength = 25.0f; // ~25 Earth days
    sun.yearLength = 0.0f;
    sun.gravity = 274.0f;
    sun.wikiUrl = "https://en.wikipedia.org/wiki/Sun";
    sun.color[0] = 1.0f; sun.color[1] = 1.0f; sun.color[2] = 0.9f;
    sun.textureID = loadTexture("2k_sun.jpg");
    if (sun.textureID == 0) {
        sun.textureID = createFallbackTexture(1.0f, 0.9f, 0.3f);
    }

    // Mercury
    Planet mercury;
    mercury.name = "Mercury";
    mercury.radius = 3.0f;
    mercury.orbitRadius = 40.0f;
    mercury.orbitSpeed = 4.15f;
    mercury.rotationSpeed = 1.0f;
    mercury.angle = 0.0f;
    mercury.axisRotation = 0.0f;
    mercury.tilt = 0.034f;
    mercury.hasRings = false;
    mercury.textureRotation = 0.0f;
    mercury.dayLength = 58.6f;
    mercury.yearLength = 88.0f;
    mercury.gravity = 3.7f;
    mercury.wikiUrl = "https://en.wikipedia.org/wiki/Mercury_(planet)";
    mercury.color[0] = 0.7f; mercury.color[1] = 0.7f; mercury.color[2] = 0.7f;
    mercury.textureID = loadTexture("2k_mercury.jpg");
    if (mercury.textureID == 0) {
        mercury.textureID = createFallbackTexture(0.55f, 0.55f, 0.57f);
    }
    planets.push_back(mercury);

    // Venus
    Planet venus;
    venus.name = "Venus";
    venus.radius = 4.5f;
    venus.orbitRadius = 55.0f;
    venus.orbitSpeed = 1.62f;
    venus.rotationSpeed = 0.4f;
    venus.angle = 0.0f;
    venus.axisRotation = 0.0f;
    venus.tilt = 2.64f;
    venus.hasRings = false;
    venus.textureRotation = 0.0f;
    venus.dayLength = 243.0f;
    venus.yearLength = 225.0f;
    venus.gravity = 8.87f;
    venus.wikiUrl = "https://en.wikipedia.org/wiki/Venus";
    venus.color[0] = 0.95f; venus.color[1] = 0.9f; venus.color[2] = 0.8f;
    venus.textureID = loadTexture("2k_venus_surface.jpg");
    if (venus.textureID == 0) {
        venus.textureID = createFallbackTexture(0.95f, 0.88f, 0.7f);
    }
    planets.push_back(venus);

    // Earth
    Planet earth;
    earth.name = "Earth";
    earth.radius = 5.0f;
    earth.orbitRadius = 75.0f;
    earth.orbitSpeed = 1.0f;
    earth.rotationSpeed = 1.0f;
    earth.angle = 0.0f;
    earth.axisRotation = 0.0f;
    earth.tilt = 23.44f;
    earth.hasRings = false;
    earth.textureRotation = 0.0f;
    earth.dayLength = 1.0f;
    earth.yearLength = 365.25f;
    earth.gravity = 9.81f;
    earth.wikiUrl = "https://en.wikipedia.org/wiki/Earth";
    earth.color[0] = 0.3f; earth.color[1] = 0.6f; earth.color[2] = 0.9f;
    earth.textureID = loadTexture("2k_earth_daymap.jpg");
    if (earth.textureID == 0) {
        earth.textureID = createFallbackTexture(0.25f, 0.5f, 0.85f);
    }

    // Add Moon to Earth
    Moon moon;
    moon.name = "Moon";
    moon.radius = 1.3f;
    moon.orbitRadius = 10.0f;
    moon.orbitSpeed = 3.0f;
    moon.angle = 0.0f;
    moon.color[0] = 0.7f; moon.color[1] = 0.7f; moon.color[2] = 0.7f;
    moon.textureID = loadTexture("2k_moon.jpg");
    if (moon.textureID == 0) {
        moon.textureID = createFallbackTexture(0.7f, 0.7f, 0.7f);
    }
    earth.moons.push_back(moon);

    planets.push_back(earth);

    // Mars
    Planet mars;
    mars.name = "Mars";
    mars.radius = 4.0f;
    mars.orbitRadius = 95.0f;
    mars.orbitSpeed = 0.53f;
    mars.rotationSpeed = 1.0f;
    mars.angle = 0.0f;
    mars.axisRotation = 0.0f;
    mars.tilt = 25.19f;
    mars.textureRotation = 0.0f;
    mars.hasRings = false;
    mars.dayLength = 1.03f;
    mars.yearLength = 687.0f;
    mars.gravity = 3.71f;
    mars.wikiUrl = "https://en.wikipedia.org/wiki/Mars";
    mars.color[0] = 0.85f; mars.color[1] = 0.4f; mars.color[2] = 0.3f;
    mars.textureID = loadTexture("2k_mars.jpg");
    if (mars.textureID == 0) {
        mars.textureID = createFallbackTexture(0.85f, 0.35f, 0.25f);
    }
    planets.push_back(mars);

    // Jupiter
    Planet jupiter;
    jupiter.name = "Jupiter";
    jupiter.radius = 12.0f;
    jupiter.orbitRadius = 130.0f;
    jupiter.orbitSpeed = 0.084f;
    jupiter.rotationSpeed = 2.4f;
    jupiter.angle = 0.0f;
    jupiter.axisRotation = 0.0f;
    jupiter.tilt = 3.13f;
    jupiter.textureRotation = 90.0f;
    jupiter.hasRings = false;
    jupiter.dayLength = 0.41f;
    jupiter.yearLength = 4333.0f;
    jupiter.gravity = 24.79f;
    jupiter.wikiUrl = "https://en.wikipedia.org/wiki/Jupiter";
    jupiter.color[0] = 0.85f; jupiter.color[1] = 0.7f; jupiter.color[2] = 0.6f;
    jupiter.textureID = loadTexture("2k_jupiter.jpg");
    if (jupiter.textureID == 0) {
        jupiter.textureID = createFallbackTexture(0.85f, 0.65f, 0.45f);
    }
    planets.push_back(jupiter);

    // Saturn
    Planet saturn;
    saturn.name = "Saturn";
    saturn.radius = 10.0f;
    saturn.orbitRadius = 170.0f;
    saturn.orbitSpeed = 0.034f;
    saturn.rotationSpeed = 2.2f;
    saturn.angle = 0.0f;
    saturn.axisRotation = 0.0f;
    saturn.tilt = 26.73f;
    saturn.hasRings = true;
    saturn.textureRotation = 0.0f;
    saturn.ringInnerRadius = 14.0f;
    saturn.ringOuterRadius = 24.0f;
    saturn.dayLength = 0.45f;
    saturn.yearLength = 10759.0f;
    saturn.gravity = 10.44f;
    saturn.wikiUrl = "https://en.wikipedia.org/wiki/Saturn";
    saturn.color[0] = 0.9f; saturn.color[1] = 0.85f; saturn.color[2] = 0.7f;
    saturn.textureID = loadTexture("2k_saturn.jpg");
    if (saturn.textureID == 0) {
        saturn.textureID = createFallbackTexture(0.92f, 0.85f, 0.65f);
    }

    // Load ring texture with alpha channel
    saturn.ringTextureID = loadTexture("2k_saturn_ring_alpha.png", true);
    if (saturn.ringTextureID == 0) {
        const int size = 256;
        unsigned char* ringData = new unsigned char[size * size * 4];
        for (int i = 0; i < size; i++) {
            for (int j = 0; j < size; j++) {
                int idx = (i * size + j) * 4;
                float u = j / (float)size;
                float bands = sin(u * 40.0f) * 0.3f + 0.7f;
                ringData[idx] = (unsigned char)(230 * bands);
                ringData[idx + 1] = (unsigned char)(210 * bands);
                ringData[idx + 2] = (unsigned char)(170 * bands);
                float v = i / (float)size;
                float alpha = 1.0f - fabs(v - 0.5f) * 2.0f;
                ringData[idx + 3] = (unsigned char)(alpha * bands * 255);
            }
        }
        glGenTextures(1, &saturn.ringTextureID);
        glBindTexture(GL_TEXTURE_2D, saturn.ringTextureID);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, size, size, 0, GL_RGBA, GL_UNSIGNED_BYTE, ringData);
        delete[] ringData;
    }

    planets.push_back(saturn);

    // Uranus
    Planet uranus;
    uranus.name = "Uranus";
    uranus.radius = 7.0f;
    uranus.orbitRadius = 210.0f;
    uranus.orbitSpeed = 0.012f;
    uranus.rotationSpeed = 1.4f;
    uranus.angle = 0.0f;
    uranus.axisRotation = 0.0f;
    uranus.tilt = 97.77f;
    uranus.hasRings = false;
    uranus.textureRotation = 0.0f;
    uranus.dayLength = 0.72f;
    uranus.yearLength = 30687.0f;
    uranus.gravity = 8.87f;
    uranus.wikiUrl = "https://en.wikipedia.org/wiki/Uranus";
    uranus.color[0] = 0.6f; uranus.color[1] = 0.8f; uranus.color[2] = 0.85f;
    uranus.textureID = loadTexture("2k_uranus.jpg");
    if (uranus.textureID == 0) {
        uranus.textureID = createFallbackTexture(0.6f, 0.8f, 0.85f);
    }
    planets.push_back(uranus);

    // Neptune
    Planet neptune;
    neptune.name = "Neptune";
    neptune.radius = 6.5f;
    neptune.orbitRadius = 250.0f;
    neptune.orbitSpeed = 0.006f;
    neptune.rotationSpeed = 1.5f;
    neptune.angle = 0.0f;
    neptune.axisRotation = 0.0f;
    neptune.tilt = 28.32f;
    neptune.textureRotation = 0.0f;
    neptune.hasRings = false;
    neptune.dayLength = 0.67f;
    neptune.yearLength = 60190.0f;
    neptune.gravity = 11.15f;
    neptune.wikiUrl = "https://en.wikipedia.org/wiki/Neptune";
    neptune.color[0] = 0.3f; neptune.color[1] = 0.4f; neptune.color[2] = 0.9f;
    neptune.textureID = loadTexture("2k_neptune.jpg");
    if (neptune.textureID == 0) {
        neptune.textureID = createFallbackTexture(0.3f, 0.4f, 0.9f);
    }
    planets.push_back(neptune);

    std::cout << "=== Texture Loading Complete ===" << std::endl;
    std::cout << "Loaded " << planets.size() << " planets" << std::endl << std::endl;
}

// Draw orbit path
void drawOrbit(float radius) {
    glDisable(GL_LIGHTING);
    glDisable(GL_TEXTURE_2D);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glColor4f(0.4f, 0.4f, 0.5f, 0.3f);
    glLineWidth(1.0f);

    glBegin(GL_LINE_LOOP);
    for (int i = 0; i < 100; i++) {
        float angle = 2.0f * M_PI * i / 100.0f;
        glVertex3f(radius * cos(angle), 0.0f, radius * sin(angle));
    }
    glEnd();

    glDisable(GL_BLEND);
    glEnable(GL_LIGHTING);
}

// Draw planet rings
void drawRings(float innerRadius, float outerRadius, GLuint textureID) {
    glEnable(GL_TEXTURE_2D);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glBindTexture(GL_TEXTURE_2D, textureID);

    const int segments = 180;

    glBegin(GL_QUAD_STRIP);
    for (int i = 0; i <= segments; i++) {
        float angle = 2.0f * M_PI * i / segments;
        float c = cos(angle);
        float s = sin(angle);

        // Texture wraps around ring circumference
        float u = (float)i / segments;

        // Inner edge
        glTexCoord2f(u, 0.0f);
        glVertex3f(innerRadius * c, 0.0f, innerRadius * s);

        // Outer edge
        glTexCoord2f(u, 1.0f);
        glVertex3f(outerRadius * c, 0.0f, outerRadius * s);
    }
    glEnd();

    glDisable(GL_BLEND);
    glDisable(GL_TEXTURE_2D);
}



// Draw textured sphere
void drawTexturedSphere(float radius, GLuint textureID) {
    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, textureID);

    glPushMatrix();

    // This aligns equirectangular textures correctly
    glRotatef(-90.0f, 1.0f, 0.0f, 0.0f); // FIX vertical flip
    glRotatef(180.0f, 0.0f, 1.0f, 0.0f); // FIX longitude direction (optional but recommended)

    GLUquadric* quad = gluNewQuadric();
    gluQuadricTexture(quad, GL_TRUE);
    gluQuadricNormals(quad, GLU_SMOOTH);
    gluSphere(quad, radius, 48, 48);
    gluDeleteQuadric(quad);

    glPopMatrix();
    glDisable(GL_TEXTURE_2D);
}


// Draw 2D text on screen
void drawText(float x, float y, const char* text, void* font = GLUT_BITMAP_HELVETICA_12) {
    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    gluOrtho2D(0, windowWidth, 0, windowHeight);

    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();

    glDisable(GL_LIGHTING);
    glDisable(GL_DEPTH_TEST);
    glColor3f(1.0f, 1.0f, 1.0f);

    glRasterPos2f(x, y);
    for (const char* c = text; *c != '\0'; c++) {
        glutBitmapCharacter(font, *c);
    }

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_LIGHTING);

    glPopMatrix();
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);
}

// Draw gravity simulation
void drawGravitySimulation(const Planet& p) {
    if (!showGravitySimulation) return;

    glDisable(GL_LIGHTING);
    glDisable(GL_TEXTURE_2D);

    // Draw ball
    glPushMatrix();
    glTranslatef(p.radius + 5.0f, gravityBallY, 0.0f);
    glColor3f(1.0f, 0.3f, 0.3f);
    glutSolidSphere(0.5f, 16, 16);
    glPopMatrix();

    // Draw ground reference
    glColor3f(0.5f, 0.5f, 0.5f);
    glBegin(GL_LINES);
    glVertex3f(p.radius + 3.0f, 0.0f, -2.0f);
    glVertex3f(p.radius + 3.0f, 0.0f, 2.0f);
    glVertex3f(p.radius + 7.0f, 0.0f, -2.0f);
    glVertex3f(p.radius + 7.0f, 0.0f, 2.0f);
    glEnd();

    glEnable(GL_LIGHTING);
}

// Get planet position in 3D space
void getPlanetPosition(int index, float& x, float& y, float& z) {
    if (index < 0 || index >= (int)planets.size()) {
        x = y = z = 0.0f;
        return;
    }

    const Planet& p = planets[index];
    x = p.orbitRadius * cos(p.angle);
    y = 0.0f;
    z = p.orbitRadius * sin(p.angle);
}

// Check if mouse is hovering over a planet
int checkPlanetHover(int mx, int my) {
    GLint viewport[4];
    GLdouble modelview[16];
    GLdouble projection[16];
    glGetIntegerv(GL_VIEWPORT, viewport);
    glGetDoublev(GL_MODELVIEW_MATRIX, modelview);
    glGetDoublev(GL_PROJECTION_MATRIX, projection);

    for (size_t i = 0; i < planets.size(); i++) {
        float px = planets[i].orbitRadius * cos(planets[i].angle);
        float py = 0.0f;
        float pz = planets[i].orbitRadius * sin(planets[i].angle);

        GLdouble winX, winY, winZ;
        gluProject(px, py, pz, modelview, projection, viewport, &winX, &winY, &winZ);

        winY = viewport[3] - winY;

        float dx = mx - winX;
        float dy = my - winY;
        float dist = sqrt(dx*dx + dy*dy);

        if (dist < 30.0f) {
            return i;
        }
    }

    return -1;
}

// Smooth interpolation function (ease-in-out)
float smoothStep(float t) {
    return t * t * (3.0f - 2.0f * t);
}

// Start camera animation to focus on planet
void startFocusAnimation(int planetIndex) {
    if (planetIndex < 0 || planetIndex >= (int)planets.size()) {
        // Reset to default view
        startCameraDistance = cameraDistance;
        startCameraAngleX = cameraAngleX;
        startCameraAngleY = cameraAngleY;
        startCameraZoom = cameraZoom;

        targetCameraDistance = 250.0f;
        targetCameraAngleX = 30.0f;
        targetCameraAngleY = 45.0f;
        targetCameraZoom = 1.0f;

        focusedPlanetIndex = -1;
        showGravitySimulation = false;
    } else {
        startCameraDistance = cameraDistance;
        startCameraAngleX = cameraAngleX;
        startCameraAngleY = cameraAngleY;
        startCameraZoom = cameraZoom;

        // Calculate target camera position for focused planet
        float distance = planets[planetIndex].radius * 3.5f;
        if (distance < 15.0f) distance = 15.0f;

        targetCameraDistance = distance;
        targetCameraAngleX = 20.0f;
        targetCameraAngleY = 45.0f;
        targetCameraZoom = 1.0f;

        focusedPlanetIndex = planetIndex;
        showGravitySimulation = false;
        gravityBallY = planets[planetIndex].radius + 10.0f;
        gravityBallVelocity = 0.0f;
        gravitySimTime = 0.0f;
    }

    isCameraAnimating = true;
    animationProgress = 0.0f;
}

// Display function
void display() {
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glLoadIdentity();

    // Update camera animation
    if (isCameraAnimating) {
        animationProgress += 0.02f;
        if (animationProgress >= 1.0f) {
            animationProgress = 1.0f;
            isCameraAnimating = false;
        }

        float t = smoothStep(animationProgress);
        cameraDistance = startCameraDistance + (targetCameraDistance - startCameraDistance) * t;
        cameraAngleX = startCameraAngleX + (targetCameraAngleX - startCameraAngleX) * t;
        cameraAngleY = startCameraAngleY + (targetCameraAngleY - startCameraAngleY) * t;
        cameraZoom = startCameraZoom + (targetCameraZoom - startCameraZoom) * t;
    }

    // Camera positioning
    float lookAtX = 0.0f, lookAtY = 0.0f, lookAtZ = 0.0f;

    if (focusedPlanetIndex >= 0 && focusedPlanetIndex < (int)planets.size()) {
        getPlanetPosition(focusedPlanetIndex, lookAtX, lookAtY, lookAtZ);
    }

    float camX = lookAtX + cameraDistance * cameraZoom * sin(cameraAngleY * M_PI / 180.0f) * cos(cameraAngleX * M_PI / 180.0f);
    float camY = lookAtY + cameraDistance * cameraZoom * sin(cameraAngleX * M_PI / 180.0f);
    float camZ = lookAtZ + cameraDistance * cameraZoom * cos(cameraAngleY * M_PI / 180.0f) * cos(cameraAngleX * M_PI / 180.0f);
    if (focusedPlanetIndex >= 0) {
        Planet& p = planets[focusedPlanetIndex];

        float fx = p.orbitRadius * cos(p.angle);
        float fz = p.orbitRadius * sin(p.angle);

        gluLookAt(
            camX + fx, camY, camZ + fz,
            fx, 0.0f, fz,
            0.0f, 1.0f, 0.0f
        );
    } else {
    gluLookAt(camX, camY, camZ, lookAtX, lookAtY, lookAtZ, 0.0, 1.0, 0.0);
    }

    // Draw galaxy background only if not focused
    if (focusedPlanetIndex < 0) {
        drawGalaxy();
    }

    // Draw Sun (self-illuminated)
    if (focusedPlanetIndex < 0) {
        glPushMatrix();
        glRotatef(sun.axisRotation, 0.0f, 1.0f, 0.0f);
        glDisable(GL_LIGHTING);
        glColor3f(1.0f, 1.0f, 1.0f);
        drawTexturedSphere(sun.radius, sun.textureID);
        glEnable(GL_LIGHTING);
        glPopMatrix();
    }

    // Draw planets
    for (size_t i = 0; i < planets.size(); i++) {
        Planet& p = planets[i];

        // Skip non-focused planets when in focus mode
        if (focusedPlanetIndex >= 0 && (int)i != focusedPlanetIndex) {
            continue;
        }

        // Draw orbit
        if (showOrbits && focusedPlanetIndex < 0) {
            drawOrbit(p.orbitRadius);
        }

        // Calculate planet position
        float x = p.orbitRadius * cos(p.angle);
        float z = p.orbitRadius * sin(p.angle);

        glPushMatrix();

        if (focusedPlanetIndex == (int)i) {
            // Centered when focused
            glTranslatef(0.0f, 0.0f, 0.0f);
        } else {
            glTranslatef(x, 0.0f, z);
        }
        if (focusedPlanetIndex == (int)i) {
            glScalef(4.0f, 4.0f, 4.0f);
        }

        // Draw rings before planet
        if (p.hasRings) {
            glPushMatrix();
            glRotatef(p.tilt, 0.0f, 0.0f, 1.0f);

            GLfloat ring_mat_ambient[] = {0.4f, 0.4f, 0.35f, 0.9f};
            GLfloat ring_mat_diffuse[] = {0.9f, 0.85f, 0.7f, 0.9f};
            glMaterialfv(GL_FRONT, GL_AMBIENT, ring_mat_ambient);
            glMaterialfv(GL_FRONT, GL_DIFFUSE, ring_mat_diffuse);

            drawRings(p.ringInnerRadius, p.ringOuterRadius, p.ringTextureID);
            glPopMatrix();
        }

        // Draw planet with axis rotation
        glRotatef(p.tilt, 0.0f, 0.0f, 1.0f);
        glRotatef(p.textureRotation, 0.0f, 1.0f, 0.0f); // NEW: Apply texture rotation first
        glRotatef(p.axisRotation, 0.0f, 1.0f, 0.0f); // Then apply axis rotation


        // Set material properties
        GLfloat mat_ambient[] = {0.3f, 0.3f, 0.3f, 1.0f};
        GLfloat mat_diffuse[] = {1.0f, 1.0f, 1.0f, 1.0f};
        GLfloat mat_specular[] = {0.3f, 0.3f, 0.3f, 1.0f};
        GLfloat mat_shininess[] = {32.0f};

        glMaterialfv(GL_FRONT, GL_AMBIENT, mat_ambient);
        glMaterialfv(GL_FRONT, GL_DIFFUSE, mat_diffuse);
        glMaterialfv(GL_FRONT, GL_SPECULAR, mat_specular);
        glMaterialfv(GL_FRONT, GL_SHININESS, mat_shininess);

        glColor3f(1.0f, 1.0f, 1.0f);
        drawTexturedSphere(p.radius, p.textureID);

        // Draw gravity simulation if focused
        if (focusedPlanetIndex == (int)i) {
            drawGravitySimulation(p);
        }

        // Draw moons
        for (size_t j = 0; j < p.moons.size(); j++) {
            Moon& m = p.moons[j];

            if (showOrbits && focusedPlanetIndex < 0) {
                glDisable(GL_LIGHTING);
                glDisable(GL_TEXTURE_2D);
                glColor4f(0.3f, 0.3f, 0.4f, 0.4f);
                glBegin(GL_LINE_LOOP);
                for (int k = 0; k < 50; k++) {
                    float angle = 2.0f * M_PI * k / 50.0f;
                    glVertex3f(m.orbitRadius * cos(angle), 0.0f, m.orbitRadius * sin(angle));
                }
                glEnd();
                glEnable(GL_LIGHTING);
            }

            float mx = m.orbitRadius * cos(m.angle);
            float mz = m.orbitRadius * sin(m.angle);

            glPushMatrix();
            glTranslatef(mx, 0.0f, mz);

            GLfloat moon_ambient[] = {0.2f, 0.2f, 0.2f, 1.0f};
            GLfloat moon_diffuse[] = {1.0f, 1.0f, 1.0f, 1.0f};
            glMaterialfv(GL_FRONT, GL_AMBIENT, moon_ambient);
            glMaterialfv(GL_FRONT, GL_DIFFUSE, moon_diffuse);

            glColor3f(1.0f, 1.0f, 1.0f);
            drawTexturedSphere(m.radius, m.textureID);
            glPopMatrix();
        }

        glPopMatrix();
    }

    // Draw hover tooltip
    if (hoveredPlanetIndex >= 0 && hoveredPlanetIndex < (int)planets.size() && focusedPlanetIndex < 0) {
        const Planet& p = planets[hoveredPlanetIndex];

        std::ostringstream oss;
        oss << p.name << "\n";
        oss << "Radius: " << std::fixed << std::setprecision(1) << p.radius << " units\n";
        oss << "Day Length: " << std::fixed << std::setprecision(2) << p.dayLength << " Earth days\n";
        oss << "Year Length: " << std::fixed << std::setprecision(1) << p.yearLength << " Earth days\n";
        oss << "Gravity: " << std::fixed << std::setprecision(2) << p.gravity << " m/s²\n";
        oss << "Local Time: " << getPlanetTimeString(p);

        std::string info = oss.str();
        float y = mouseY + 20.0f;

        size_t start = 0;
        size_t end = info.find('\n');
        while (end != std::string::npos) {
            std::string line = info.substr(start, end - start);
            drawText(mouseX + 15.0f, windowHeight - y, line.c_str(), GLUT_BITMAP_9_BY_15);
            y += 18.0f;
            start = end + 1;
            end = info.find('\n', start);
        }
        if (start < info.length()) {
            std::string line = info.substr(start);
            drawText(mouseX + 15.0f, windowHeight - y, line.c_str(), GLUT_BITMAP_9_BY_15);
        }
    }

    // Draw focused planet info
    if (focusedPlanetIndex >= 0 && focusedPlanetIndex < (int)planets.size()) {
        const Planet& p = planets[focusedPlanetIndex];

        std::ostringstream oss;
        oss << "═══ " << p.name << " ═══";
        drawText(20.0f, windowHeight - 30.0f, oss.str().c_str(), GLUT_BITMAP_HELVETICA_18);

        oss.str("");
        oss << "Radius: " << std::fixed << std::setprecision(1) << p.radius << " units";
        drawText(20.0f, windowHeight - 55.0f, oss.str().c_str());

        oss.str("");
        oss << "Day Length: " << std::fixed << std::setprecision(2) << p.dayLength << " Earth days";
        drawText(20.0f, windowHeight - 75.0f, oss.str().c_str());

        oss.str("");
        oss << "Year Length: " << std::fixed << std::setprecision(1) << p.yearLength << " Earth days";
        drawText(20.0f, windowHeight - 95.0f, oss.str().c_str());

        oss.str("");
        oss << "Gravity: " << std::fixed << std::setprecision(2) << p.gravity << " m/s²";
        drawText(20.0f, windowHeight - 115.0f, oss.str().c_str());

        oss.str("");
        oss << "Local Time: " << getPlanetTimeString(p);
        drawText(20.0f, windowHeight - 135.0f, oss.str().c_str());

        drawText(20.0f, windowHeight - 165.0f, "Press 'W' for Wikipedia");
        drawText(20.0f, windowHeight - 185.0f, "Press 'G' to toggle gravity sim");
        drawText(20.0f, windowHeight - 205.0f, "Press 'R' to return to solar system");

        if (showGravitySimulation) {
            oss.str("");
            oss << "Gravity Sim: Ball falling at " << std::fixed << std::setprecision(2) << p.gravity << " m/s²";
            drawText(20.0f, windowHeight - 235.0f, oss.str().c_str());
        }
    }

    glutSwapBuffers();
}

// Update animation
void update(int value) {
    const float deltaTime = 0.016f;
    time_elapsed += deltaTime * animationSpeed;

    // --- Sun rotation (only if nothing is focused) ---
    if (focusedPlanetIndex < 0) {
        sun.axisRotation += sun.rotationSpeed * animationSpeed;
        if (sun.axisRotation > 360.0f) sun.axisRotation -= 360.0f;
    }

    for (size_t i = 0; i < planets.size(); i++) {
        Planet& p = planets[i];

        // ---- ORBIT (freeze when focused) ----
        if (focusedPlanetIndex < 0) {
            p.angle += p.orbitSpeed * deltaTime * animationSpeed;
            if (p.angle > 2.0f * M_PI) p.angle -= 2.0f * M_PI;
        }

        // ---- AXIS ROTATION (stop only focused planet) ----
        if (focusedPlanetIndex != (int)i && p.dayLength > 0.0f) {
            float rotationPerSecond = 360.0f / p.dayLength; // degrees per second
            p.axisRotation += rotationPerSecond * deltaTime * animationSpeed;
            if (p.axisRotation > 360.0f) p.axisRotation -= 360.0f;
        }

        // ---- MOONS (freeze when focused) ----
        if (focusedPlanetIndex < 0) {
            for (auto& m : p.moons) {
                m.angle += m.orbitSpeed * deltaTime * animationSpeed;
                if (m.angle > 2.0f * M_PI) m.angle -= 2.0f * M_PI;
            }
        }
    }

    // ---- GRAVITY SIMULATION (only when focused) ----
    if (showGravitySimulation && focusedPlanetIndex >= 0) {
        const Planet& p = planets[focusedPlanetIndex];

        gravityBallVelocity -= p.gravity * deltaTime * 0.3f;
        gravityBallY += gravityBallVelocity;

        if (gravityBallY <= p.radius) {
            gravityBallY = p.radius + 10.0f;
            gravityBallVelocity = 0.0f;
        }
    }

    glutPostRedisplay();
    glutTimerFunc(16, update, 0);
}


// Keyboard handler
void keyboard(unsigned char key, int x, int y) {
    switch (key) {
        case 27: // ESC
            exit(0);
            break;
        case 'o':
        case 'O':
            showOrbits = !showOrbits;
            std::cout << "Orbits: " << (showOrbits ? "ON" : "OFF") << std::endl;
            break;
        case '+':
        case '=':
            animationSpeed += 0.1f;
            std::cout << "Speed: " << animationSpeed << "x" << std::endl;
            break;
        case '-':
        case '_':
            animationSpeed = std::max(0.1f, animationSpeed - 0.1f);
            std::cout << "Speed: " << animationSpeed << "x" << std::endl;
            break;
        case 'r':
        case 'R':
            if (focusedPlanetIndex >= 0) {
                std::cout << "Returning to solar system view" << std::endl;
                startFocusAnimation(-1);
            } else {
                cameraAngleX = 30.0f;
                cameraAngleY = 45.0f;
                cameraZoom = 1.0f;
                std::cout << "Camera reset" << std::endl;
            }
            break;
        case 'w':
        case 'W':
            if (focusedPlanetIndex >= 0 && focusedPlanetIndex < (int)planets.size()) {
                std::cout << "Opening Wikipedia for " << planets[focusedPlanetIndex].name << std::endl;
                openURL(planets[focusedPlanetIndex].wikiUrl);
            }
            break;
        case 'g':
        case 'G':
            if (focusedPlanetIndex >= 0) {
                showGravitySimulation = !showGravitySimulation;
                if (showGravitySimulation) {
                    gravityBallY = planets[focusedPlanetIndex].radius + 10.0f;
                    gravityBallVelocity = 0.0f;
                    gravitySimTime = 0.0f;
                }
                std::cout << "Gravity simulation: " << (showGravitySimulation ? "ON" : "OFF") << std::endl;
            }
            break;
    }
    glutPostRedisplay();
}

// Mouse handler
void mouse(int button, int state, int x, int y) {
    if (button == GLUT_LEFT_BUTTON) {
        if (state == GLUT_DOWN) {
            // Check if clicking on a planet
            if (focusedPlanetIndex < 0) {
                int clickedPlanet = checkPlanetHover(x, y);
                if (clickedPlanet >= 0) {
                    std::cout << "Focusing on " << planets[clickedPlanet].name << std::endl;
                    startFocusAnimation(clickedPlanet);
                    return;
                }
            }

            isMouseDragging = true;
            lastMouseX = x;
            lastMouseY = y;
        } else {
            isMouseDragging = false;
        }
    } else if (button == 3) { // Mouse wheel up
        cameraZoom *= 0.9f;
        if (cameraZoom < 0.1f) cameraZoom = 0.1f;
        glutPostRedisplay();
    } else if (button == 4) { // Mouse wheel down
        cameraZoom *= 1.1f;
        if (cameraZoom > 5.0f) cameraZoom = 5.0f;
        glutPostRedisplay();
    }
}

// Mouse motion handler
void mouseMotion(int x, int y) {
    mouseX = x;
    mouseY = y;

    if (isMouseDragging) {
        cameraAngleY += (x - lastMouseX) * 0.5f;
        cameraAngleX += (y - lastMouseY) * 0.5f;

        if (cameraAngleX > 89.0f) cameraAngleX = 89.0f;
        if (cameraAngleX < -89.0f) cameraAngleX = -89.0f;

        lastMouseX = x;
        lastMouseY = y;
        glutPostRedisplay();
    }
}

// Passive mouse motion handler (for hover detection)
void passiveMouseMotion(int x, int y) {
    mouseX = x;
    mouseY = y;

    if (focusedPlanetIndex < 0) {
        int newHover = checkPlanetHover(x, y);
        if (newHover != hoveredPlanetIndex) {
            hoveredPlanetIndex = newHover;
            glutPostRedisplay();
        }
    }
}

// Reshape handler
void reshape(int w, int h) {
    if (h == 0) h = 1;
    windowWidth = w;
    windowHeight = h;
    glViewport(0, 0, w, h);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluPerspective(45.0, (double)w / (double)h, 1.0, 3000.0);
    glMatrixMode(GL_MODELVIEW);
}

// Initialize OpenGL
void initGL() {
    glClearColor(0.0f, 0.0f, 0.02f, 1.0f);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);
    glEnable(GL_LIGHTING);
    glEnable(GL_LIGHT0);
    glEnable(GL_COLOR_MATERIAL);
    glColorMaterial(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE);

    // Light properties - Sun at center
    GLfloat light_pos[] = {0.0f, 0.0f, 0.0f, 1.0f};
    GLfloat light_ambient[] = {0.1f, 0.1f, 0.12f, 1.0f};
    GLfloat light_diffuse[] = {1.0f, 0.98f, 0.95f, 1.0f};
    GLfloat light_specular[] = {1.0f, 1.0f, 1.0f, 1.0f};

    glLightfv(GL_LIGHT0, GL_POSITION, light_pos);
    glLightfv(GL_LIGHT0, GL_AMBIENT, light_ambient);
    glLightfv(GL_LIGHT0, GL_DIFFUSE, light_diffuse);
    glLightfv(GL_LIGHT0, GL_SPECULAR, light_specular);

    // Realistic light attenuation
    glLightf(GL_LIGHT0, GL_CONSTANT_ATTENUATION, 1.0f);
    glLightf(GL_LIGHT0, GL_LINEAR_ATTENUATION, 0.0005f);
    glLightf(GL_LIGHT0, GL_QUADRATIC_ATTENUATION, 0.00001f);

    glShadeModel(GL_SMOOTH);
    glEnable(GL_NORMALIZE);
    glHint(GL_PERSPECTIVE_CORRECTION_HINT, GL_NICEST);

    // Enable texture features
    glEnable(GL_TEXTURE_2D);

    initGalaxy();
    initTextures();
}

// Print help
void printHelp() {
    std::cout << "\n╔════════════════════════════════════════════════════╗" << std::endl;
    std::cout << "║   ENHANCED TEXTURE-MAPPED SOLAR SYSTEM             ║" << std::endl;
    std::cout << "║   Author: Э.Намуундарь                             ║" << std::endl;
    std::cout << "╚════════════════════════════════════════════════════╝" << std::endl;
    std::cout << "\n📁 Required texture files (2048x1024 resolution):" << std::endl;
    std::cout << "   Place these in the same directory as the executable:" << std::endl;
    std::cout << "   • 2k_sun.jpg, 2k_mercury.jpg, 2k_venus_surface.jpg" << std::endl;
    std::cout << "   • 2k_earth_daymap.jpg, 2k_moon.jpg, 2k_mars.jpg" << std::endl;
    std::cout << "   • 2k_jupiter.jpg, 2k_saturn.jpg, 2k_uranus.jpg" << std::endl;
    std::cout << "   • 2k_neptune.jpg, 2k_saturn_ring_alpha.png" << std::endl;
    std::cout << "\n🌐 Download textures from:" << std::endl;
    std::cout << "   https://www.solarsystemscope.com/textures/" << std::endl;
    std::cout << "\n🎮 Controls:" << std::endl;
    std::cout << "   • Mouse hover     : Show planet details" << std::endl;
    std::cout << "   • Left click      : Focus on planet" << std::endl;
    std::cout << "   • Mouse drag      : Rotate view" << std::endl;
    std::cout << "   • Mouse wheel     : Zoom in/out" << std::endl;
    std::cout << "   • 'o' key         : Toggle orbit paths" << std::endl;
    std::cout << "   • '+' / '-' keys  : Speed up/slow down" << std::endl;
    std::cout << "   • 'r' key         : Reset camera / Return to system" << std::endl;
    std::cout << "   • 'w' key         : Open Wikipedia (when focused)" << std::endl;
    std::cout << "   • 'g' key         : Toggle gravity sim (when focused)" << std::endl;
    std::cout << "   • ESC key         : Exit program" << std::endl;
    std::cout << "\n✨ New Features:" << std::endl;
    std::cout << "   • Planets rotate on their own axis" << std::endl;
    std::cout << "   • Hover over planets to see details" << std::endl;
    std::cout << "   • Click to focus with smooth camera animation" << std::endl;
    std::cout << "   • View Wikipedia pages for each planet" << std::endl;
    std::cout << "   • Gravity simulation with falling ball" << std::endl;
    std::cout << "   • Planet-specific time systems" << std::endl;
    std::cout << "═══════════════════════════════════════════════════════\n" << std::endl;
}

// Main function
int main(int argc, char** argv) {
    printHelp();

    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB | GLUT_DEPTH);
    glutInitWindowSize(windowWidth, windowHeight);
    glutInitWindowPosition(100, 50);
    glutCreateWindow("Enhanced Solar System - Э.Намуундарь");

    initGL();

    glutDisplayFunc(display);
    glutReshapeFunc(reshape);
    glutKeyboardFunc(keyboard);
    glutMouseFunc(mouse);
    glutMotionFunc(mouseMotion);
    glutPassiveMotionFunc(passiveMouseMotion);
    glutTimerFunc(16, update, 0);

    std::cout << "🚀 Starting Enhanced Solar System simulation..." << std::endl;
    std::cout << "   Hover over planets for info, click to focus!" << std::endl;

    glutMainLoop();
    return 0;
}
