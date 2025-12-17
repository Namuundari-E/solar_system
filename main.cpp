/*
 * Texture-Mapped Solar System with Real NASA Textures
 * Author: Э.Намуундарь
 *
 * Compilation:
 * g++ -o solar_system solar_system.cpp -lglut -lGLU -lGL
 *
 * Required texture files (place in same directory as executable):
 * - 2k_sun.jpg
 * - 2k_mercury.jpg
 * - 2k_venus_surface.jpg
 * - 2k_earth_daymap.jpg
 * - 2k_moon.jpg
 * - 2k_mars.jpg
 * - 2k_jupiter.jpg
 * - 2k_saturn.jpg
 * - 2k_uranus.jpg
 * - 2k_neptune.jpg
 * - 2k_saturn_ring_alpha.png
 *
 * Dependencies:
 * - FreeGLUT
 * - OpenGL
 * - stb_image.h (included, header-only library)
 *
 * Download stb_image.h from:
 * https://raw.githubusercontent.com/nothings/stb/master/stb_image.h
 *
 * Controls:
 * - Mouse drag: Rotate view
 * - Mouse wheel: Zoom in/out
 * - 'o': Toggle orbits
 * - '+/-': Increase/decrease animation speed
 * - 'r': Reset view
 * - ESC: Exit
 */

#include <GL/glut.h>
#include <GL/glu.h>
#include <cmath>
#include <iostream>
#include <vector>
#include <string>

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

// Mouse control
int lastMouseX = 0;
int lastMouseY = 0;
bool isMouseDragging = false;

// Animation
float animationSpeed = 1.0f;
float time_elapsed = 0.0f;
bool showOrbits = true;

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

// Planet structure
struct Planet {
    const char* name;
    float radius;
    float orbitRadius;
    float orbitSpeed;
    float rotationSpeed;
    float angle;
    float tilt;
    GLuint textureID;
    float color[3];
    bool hasRings;
    float ringInnerRadius;
    float ringOuterRadius;
    GLuint ringTextureID;
    std::vector<Moon> moons;
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

// Load texture from file using stb_image
GLuint loadTexture(const char* filename, bool hasAlpha = false) {
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

    // Set texture parameters
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    // Determine format based on channels
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

    // Upload texture data
    glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format, GL_UNSIGNED_BYTE, data);

    // Generate mipmaps
    gluBuild2DMipmaps(GL_TEXTURE_2D, format, width, height, format, GL_UNSIGNED_BYTE, data);

    // Free image data
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

    // Create dense star field with galaxy-like distribution
    for (int i = 0; i < 8000; i++) {
        Star s;

        // Create spiral galaxy pattern
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

// Initialize textures
void initTextures() {
    std::cout << "\n=== Loading Planet Textures ===" << std::endl;

    // Sun
    sun.name = "Sun";
    sun.radius = 20.0f;
    sun.orbitRadius = 0.0f;
    sun.orbitSpeed = 0.0f;
    sun.rotationSpeed = 0.1f;
    sun.angle = 0.0f;
    sun.tilt = 0.0f;
    sun.hasRings = false;
    sun.color[0] = 1.0f; sun.color[1] = 1.0f; sun.color[2] = 0.9f;
    sun.textureID = loadTexture("2k_sun.jpg");
    if (sun.textureID == 0) {
        std::cout << "Using fallback texture for Sun" << std::endl;
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
    mercury.tilt = 0.034f;
    mercury.hasRings = false;
    mercury.color[0] = 0.7f; mercury.color[1] = 0.7f; mercury.color[2] = 0.7f;
    mercury.textureID = loadTexture("2k_mercury.jpg");
    if (mercury.textureID == 0) {
        std::cout << "Using fallback texture for Mercury" << std::endl;
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
    venus.tilt = 2.64f;
    venus.hasRings = false;
    venus.color[0] = 0.95f; venus.color[1] = 0.9f; venus.color[2] = 0.8f;
    venus.textureID = loadTexture("2k_venus_surface.jpg");
    if (venus.textureID == 0) {
        std::cout << "Using fallback texture for Venus" << std::endl;
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
    earth.tilt = 23.44f;
    earth.hasRings = false;
    earth.color[0] = 0.3f; earth.color[1] = 0.6f; earth.color[2] = 0.9f;
    earth.textureID = loadTexture("2k_earth_daymap.jpg");
    if (earth.textureID == 0) {
        std::cout << "Using fallback texture for Earth" << std::endl;
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
        std::cout << "Using fallback texture for Moon" << std::endl;
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
    mars.tilt = 25.19f;
    mars.hasRings = false;
    mars.color[0] = 0.85f; mars.color[1] = 0.4f; mars.color[2] = 0.3f;
    mars.textureID = loadTexture("2k_mars.jpg");
    if (mars.textureID == 0) {
        std::cout << "Using fallback texture for Mars" << std::endl;
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
    jupiter.tilt = 3.13f;
    jupiter.hasRings = false;
    jupiter.color[0] = 0.85f; jupiter.color[1] = 0.7f; jupiter.color[2] = 0.6f;
    jupiter.textureID = loadTexture("2k_jupiter.jpg");
    if (jupiter.textureID == 0) {
        std::cout << "Using fallback texture for Jupiter" << std::endl;
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
    saturn.tilt = 26.73f;
    saturn.hasRings = true;
    saturn.ringInnerRadius = 14.0f;
    saturn.ringOuterRadius = 24.0f;
    saturn.color[0] = 0.9f; saturn.color[1] = 0.85f; saturn.color[2] = 0.7f;
    saturn.textureID = loadTexture("2k_saturn.jpg");
    if (saturn.textureID == 0) {
        std::cout << "Using fallback texture for Saturn" << std::endl;
        saturn.textureID = createFallbackTexture(0.92f, 0.85f, 0.65f);
    }

    // Load ring texture with alpha channel
    saturn.ringTextureID = loadTexture("2k_saturn_ring_alpha.png", true);
    if (saturn.ringTextureID == 0) {
        std::cout << "Using fallback texture for Saturn rings" << std::endl;
        // Create simple ring texture as fallback
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
    uranus.tilt = 97.77f;
    uranus.hasRings = false;
    uranus.color[0] = 0.6f; uranus.color[1] = 0.8f; uranus.color[2] = 0.85f;
    uranus.textureID = loadTexture("2k_uranus.jpg");
    if (uranus.textureID == 0) {
        std::cout << "Using fallback texture for Uranus" << std::endl;
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
    neptune.tilt = 28.32f;
    neptune.hasRings = false;
    neptune.color[0] = 0.3f; neptune.color[1] = 0.4f; neptune.color[2] = 0.9f;
    neptune.textureID = loadTexture("2k_neptune.jpg");
    if (neptune.textureID == 0) {
        std::cout << "Using fallback texture for Neptune" << std::endl;
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

    glPushMatrix();
    glRotatef(90.0f, 1.0f, 0.0f, 0.0f);

    int segments = 100;
    glBegin(GL_QUAD_STRIP);
    for (int i = 0; i <= segments; i++) {
        float angle = 2.0f * M_PI * i / segments;
        float c = cos(angle);
        float s = sin(angle);
        float u = (float)i / segments;

        glTexCoord2f(u, 0.0f);
        glVertex3f(innerRadius * c, innerRadius * s, 0.0f);
        glTexCoord2f(u, 1.0f);
        glVertex3f(outerRadius * c, outerRadius * s, 0.0f);
    }
    glEnd();

    glPopMatrix();
    glDisable(GL_BLEND);
    glDisable(GL_TEXTURE_2D);
}

// Draw textured sphere
void drawTexturedSphere(float radius, GLuint textureID) {
    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, textureID);

    GLUquadric* quad = gluNewQuadric();
    gluQuadricTexture(quad, GL_TRUE);
    gluQuadricNormals(quad, GLU_SMOOTH);
    gluSphere(quad, radius, 48, 48);
    gluDeleteQuadric(quad);

    glDisable(GL_TEXTURE_2D);
}

// Display function
void display() {
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glLoadIdentity();

    // Camera positioning
    float camX = cameraDistance * cameraZoom * sin(cameraAngleY * M_PI / 180.0f) * cos(cameraAngleX * M_PI / 180.0f);
    float camY = cameraDistance * cameraZoom * sin(cameraAngleX * M_PI / 180.0f);
    float camZ = cameraDistance * cameraZoom * cos(cameraAngleY * M_PI / 180.0f) * cos(cameraAngleX * M_PI / 180.0f);

    gluLookAt(camX, camY, camZ, 0.0, 0.0, 0.0, 0.0, 1.0, 0.0);

    // Draw galaxy background
    drawGalaxy();

    // Draw Sun (self-illuminated)
    glPushMatrix();
    glRotatef(sun.angle, 0.0f, 1.0f, 0.0f);
    glDisable(GL_LIGHTING);
    glColor3f(1.0f, 1.0f, 1.0f);
    drawTexturedSphere(sun.radius, sun.textureID);
    glEnable(GL_LIGHTING);
    glPopMatrix();

    // Draw planets
    for (size_t i = 0; i < planets.size(); i++) {
        Planet& p = planets[i];

        // Draw orbit
        if (showOrbits) {
            drawOrbit(p.orbitRadius);
        }

        // Calculate planet position
        float x = p.orbitRadius * cos(p.angle);
        float z = p.orbitRadius * sin(p.angle);

        glPushMatrix();
        glTranslatef(x, 0.0f, z);

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

        // Draw planet
        glRotatef(p.tilt, 0.0f, 0.0f, 1.0f);
        glRotatef(p.angle * p.rotationSpeed * 10.0f, 0.0f, 1.0f, 0.0f);

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

        // Draw moons
        for (size_t j = 0; j < p.moons.size(); j++) {
            Moon& m = p.moons[j];

            if (showOrbits) {
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

    glutSwapBuffers();
}

// Update animation
void update(int value) {
    time_elapsed += 0.016f * animationSpeed;

    sun.angle += sun.rotationSpeed * animationSpeed;

    for (size_t i = 0; i < planets.size(); i++) {
        planets[i].angle += planets[i].orbitSpeed * 0.01f * animationSpeed;
        if (planets[i].angle > 2.0f * M_PI) {
            planets[i].angle -= 2.0f * M_PI;
        }

        // Update moons
        for (size_t j = 0; j < planets[i].moons.size(); j++) {
            planets[i].moons[j].angle += planets[i].moons[j].orbitSpeed * 0.01f * animationSpeed;
            if (planets[i].moons[j].angle > 2.0f * M_PI) {
                planets[i].moons[j].angle -= 2.0f * M_PI;
            }
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
            cameraAngleX = 30.0f;
            cameraAngleY = 45.0f;
            cameraZoom = 1.0f;
            std::cout << "Camera reset" << std::endl;
            break;
    }
    glutPostRedisplay();
}

// Mouse handler
void mouse(int button, int state, int x, int y) {
    if (button == GLUT_LEFT_BUTTON) {
        if (state == GLUT_DOWN) {
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
    std::cout << "║   TEXTURE-MAPPED SOLAR SYSTEM                      ║" << std::endl;
    std::cout << "║   Author: Э.Намуундарь                             ║" << std::endl;
    std::cout << "╚════════════════════════════════════════════════════╝" << std::endl;
    std::cout << "\n📁 Required texture files (2048x1024 resolution):" << std::endl;
    std::cout << "   Place these in the same directory as the executable:" << std::endl;
    std::cout << "   • 2k_sun.jpg" << std::endl;
    std::cout << "   • 2k_mercury.jpg" << std::endl;
    std::cout << "   • 2k_venus_surface.jpg" << std::endl;
    std::cout << "   • 2k_earth_daymap.jpg" << std::endl;
    std::cout << "   • 2k_moon.jpg" << std::endl;
    std::cout << "   • 2k_mars.jpg" << std::endl;
    std::cout << "   • 2k_jupiter.jpg" << std::endl;
    std::cout << "   • 2k_saturn.jpg" << std::endl;
    std::cout << "   • 2k_uranus.jpg" << std::endl;
    std::cout << "   • 2k_neptune.jpg" << std::endl;
    std::cout << "   • 2k_saturn_ring_alpha.png" << std::endl;
    std::cout << "\n🌐 Download textures from:" << std::endl;
    std::cout << "   https://www.solarsystemscope.com/textures/" << std::endl;
    std::cout << "\n🎮 Controls:" << std::endl;
    std::cout << "   • Mouse drag      : Rotate view" << std::endl;
    std::cout << "   • Mouse wheel     : Zoom in/out" << std::endl;
    std::cout << "   • 'o' key         : Toggle orbit paths" << std::endl;
    std::cout << "   • '+' / '-' keys  : Speed up/slow down" << std::endl;
    std::cout << "   • 'r' key         : Reset camera" << std::endl;
    std::cout << "   • ESC key         : Exit program" << std::endl;
    std::cout << "\n🪐 Planets (in order from Sun):" << std::endl;
    std::cout << "   1. Mercury  2. Venus   3. Earth (+ Moon)" << std::endl;
    std::cout << "   4. Mars     5. Jupiter 6. Saturn (+ Rings)" << std::endl;
    std::cout << "   7. Uranus   8. Neptune" << std::endl;
    std::cout << "\n✨ Features:" << std::endl;
    std::cout << "   • Realistic NASA texture mapping" << std::endl;
    std::cout << "   • 8000+ star galaxy background" << std::endl;
    std::cout << "   • Accurate orbital mechanics" << std::endl;
    std::cout << "   • Saturn's ring system" << std::endl;
    std::cout << "   • Earth's moon with orbit" << std::endl;
    std::cout << "   • Dynamic lighting from Sun" << std::endl;
    std::cout << "═══════════════════════════════════════════════════════\n" << std::endl;
}

// Main function
int main(int argc, char** argv) {
    printHelp();

    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB | GLUT_DEPTH);
    glutInitWindowSize(windowWidth, windowHeight);
    glutInitWindowPosition(100, 50);
    glutCreateWindow("Solar System - Э.Намуундарь");

    initGL();

    glutDisplayFunc(display);
    glutReshapeFunc(reshape);
    glutKeyboardFunc(keyboard);
    glutMouseFunc(mouse);
    glutMotionFunc(mouseMotion);
    glutTimerFunc(16, update, 0);

    std::cout << "🚀 Starting Solar System simulation..." << std::endl;
    std::cout << "   Press 'o' to toggle orbits, '+/-' for speed" << std::endl;

    glutMainLoop();
    return 0;
}
