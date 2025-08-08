#pragma once
#include <algorithm>
#include <iostream>
#include <vector>
#include <random>


#define GLEW_STATIC 1
#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>


constexpr float TURN_ANGULAR_ACCEL = 20.f;   // deg/s^2
constexpr float MAX_TURN_RATE      = 90.f;  // deg/s


inline float randomPick() {
    static thread_local std::mt19937 rng(std::random_device{}());
    static std::uniform_int_distribution<int> dist(-1, 1); // -1, 0, 1
    return static_cast<float>(dist(rng));
}

class Airplane {
private:
    bool alive{true};
    glm::vec3 pos{};
    glm::vec3 vel{};
    float roll{0.f};             // degrees
    float turnrate{0.f};         // deg/s, yaw rate
    float currentTurnDir{0.f};   // -1, 0, or 1
    float dirTimer{0.f};         // seconds since last change

public:
    explicit Airplane(const glm::vec3& startPos)
        : pos(startPos)
    {
        vel = glm::vec3(0.f, 0.f, 15.f)
            + glm::vec3(2.f, 0.f, 0.f) * randomPick()
            + glm::vec3(0.f, 2.f, 0.f) * randomPick();
    }

    void update(float dt) {
        if (this->alive){
            pos += vel * dt;

            // rotate vel around global Y by the angle turned this frame
            // turnrate is deg/s -> angle this frame in radians
            const float deltaAngleRad = glm::radians(turnrate * dt);
            if (deltaAngleRad != 0.f) {
                const glm::mat3 R = glm::mat3(glm::rotate(glm::mat4(1.f), deltaAngleRad, glm::vec3(0.f, 1.f, 0.f)));
                vel = R * vel;
            }

            
            roll = (turnrate / MAX_TURN_RATE) * 60.f;


            turnrate = std::clamp(turnrate + TURN_ANGULAR_ACCEL * dt * currentTurnDir,
                                -MAX_TURN_RATE, MAX_TURN_RATE);


            dirTimer += dt;
            if (dirTimer >= .5f) {
                currentTurnDir = randomPick(); // -1, 0, or 1
                dirTimer = 0.f;
            }
        }  
    }
    void kill(){
        this->alive = false;
    }

    glm::vec3 position() const { return pos; }
    glm::vec3 velocity() const { return vel; }
    float bankRollDeg() const { return roll; }
    bool isAlive() const { return alive; }
    glm::mat4 velocityYawMatrix() const {
    glm::vec3 v = vel;
    if (glm::length(v) < 1e-5f) return glm::mat4(1.f);
    v = glm::normalize(v);
    float yaw = std::atan2(v.x, v.z);
    return glm::rotate(glm::mat4(1.f), yaw, glm::vec3(0.f, 1.f, 0.f));
}
};
