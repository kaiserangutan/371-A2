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

constexpr float SPEED      = 80.f;  // unit/s
constexpr float LIFESPAN_      = 5.f;
constexpr glm::vec3 GRAV = glm::vec3(0.f, -6.f, 0.f);




class Bullet {
private:
    bool alive{true};
    glm::vec3 pos{};
    glm::vec3 vel{};
    float age{0.f};

public:
    explicit Bullet(const glm::vec3& startPos, const glm::vec3& gunLookAt)
        : pos(startPos)
    {
        vel = SPEED * gunLookAt
            + glm::vec3(1.f * randomPick(), 1.f * randomPick(), 1.f * randomPick());
    }

    void update(float dt) {
        if (this->alive){
            pos += vel * dt;
            pos += GRAV * dt;
            age += dt;
            if (this->age > LIFESPAN_){
                alive = false;
                return;
            }

        }  
    }
    void kill(){
        this->alive = false;
    }

    glm::vec3 position() const { return pos; }
    glm::vec3 velocity() const { return vel; }
    bool isAlive() const { return alive; }
    
};
