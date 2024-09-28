#include <SDL2/SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <math.h>

#define SCREEN_WIDTH 800
#define SCREEN_HEIGHT 600
#define ANT_COUNT 1000
#define FOOD_COUNT 100
#define FOOD_DETECTION_RADIUS 50
#define FOOD_CAPTURE_RADIUS 2

#define PHEROMONE_DECAY_RATE 0.05
#define PHEROMONE_DECREASE_RATE 1
#define PHEROMONE_COUNT 480000
#define GRID_WIDTH SCREEN_WIDTH
#define GRID_HEIGHT SCREEN_HEIGHT

#define NEST_X 400
#define NEST_Y 300


#define MAX_SPEED 2
#define STEER_STRENGTH 2
#define WANDER_STRENGTH 0.1


typedef struct {
    double x;
    double y;
} Vector2D;

typedef enum {
    WANDER,
    MOVE_TO_FOOD,
    FOLLOW_PHEROMONE,
    MOVE_TO_NEST,
    
    TO_FOOD,
    TO_HOME,
    TO_HOME_NO_FOOD,
} AntState;

// Ant structure
typedef struct {
    Vector2D position;
    Vector2D velocity;
    int has_food;
    Vector2D desiredDirection;
    AntState state;
    float pheromoneStrength;
} Ant;

// Food structure
typedef struct {
    int x, y;
    int amount;
    int exists;
} Food;

// Pheromone grid
typedef struct {
	int type;
	float strength;
} Pheromone;
Pheromone pheromones[GRID_WIDTH][GRID_HEIGHT];

// Camera position and zoom factor
float zoom = 1.0f;
int camera_x = 0, camera_y = 0;

// Initialize ants with random positions and movement
void initAnts(Ant ants[], int count) {
    for (int i = 0; i < count; ++i) {
    	ants[i].position.x = rand() % SCREEN_WIDTH;
    	ants[i].position.y = rand() % SCREEN_HEIGHT;
    	ants[i].velocity.x = (rand() % 3) - 1;
    	ants[i].velocity.y = (rand() % 3) - 1;
        ants[i].has_food = 0;
        ants[i].state = TO_FOOD;
        ants[i].pheromoneStrength = 100;
    }
}

// Initialize food items at random positions
void initFood(Food foods[], int count) {
    for (int i = 0; i < count; ++i) {
        foods[i].x = rand() % SCREEN_WIDTH;
        foods[i].y = rand() % SCREEN_HEIGHT;
        foods[i].amount = 100;
        foods[i].exists = 1;
    }
}

// Distance between two points
float distance(int x1, int y1, int x2, int y2) {
    return sqrt((x1 - x2) * (x1 - x2) + (y1 - y2) * (y1 - y2));
}

// Update pheromone levels (decay and diffusion)
void updatePheromones() {
    for (int x = 0; x < GRID_WIDTH; ++x) {
        for (int y = 0; y < GRID_HEIGHT; ++y) {
            pheromones[x][y].strength *= (1.0 - PHEROMONE_DECAY_RATE);  // Decay pheromones
        }
    }
}

// Deposit pheromones on the grid
void depositPheromones(int x, int y, int type, float pheromoneStrength) {
    if (x >= 0 && x < GRID_WIDTH && y >= 0 && y < GRID_HEIGHT) {
        if (type == 0) {
            pheromones[x][y].type = 0;
            pheromones[x][y].strength += pheromoneStrength;
        } else {
            pheromones[x][y].type = 1;
            pheromones[x][y].strength += pheromoneStrength;
        }
        
    }
}


double magnitude_2d(Vector2D v) {
    return sqrt(v.x * v.x + v.y * v.y);
}

Vector2D clamp_magnitude_vector_2d(Vector2D v, double max_magnitude) {
    double current_mag = magnitude_2d(v);
    if (current_mag > max_magnitude) {
        double scale = max_magnitude / current_mag;
        v.x *= scale;
        v.y *= scale;
    }
    return v;
}


// Function to normalize a 2D vector
Vector2D normalize_vector_2d(Vector2D v) {
    double mag = magnitude_2d(v);
    if (mag == 0) {
        return v;
    }
    Vector2D normalized_vector = {v.x / mag, v.y / mag};
    return normalized_vector;
}
Vector2D multiply_vector_2d(Vector2D v, double scalar) {
    Vector2D result = {v.x * scalar, v.y * scalar};
    return result;
}
Vector2D divide_vector_2d(Vector2D v, double scalar) {
    if (scalar == 0) {
        return v;
    }
    Vector2D result;
    result.x = v.x / scalar;
    result.y = v.y / scalar;
    
    return result;
}
Vector2D add_vector_2d(Vector2D v1, Vector2D v2) {
    Vector2D result = {v1.x + v2.x, v1.y + v2.y};
    return result;
}

Vector2D add_vector_2d_scalar(Vector2D v, double scalar){
	Vector2D result = {v.x + scalar, v.y + scalar};
	return result;
}
int foodNearby(Ant* ant, Food foods[], int food_count) {
    for (int i = 0; i < food_count; ++i) {
        if (foods[i].exists && distance(ant->position.x, ant->position.y, foods[i].x, foods[i].y) < FOOD_DETECTION_RADIUS) {
            return i;
        }
    }
    return -1;
}

// Check for nearby pheromones and return the strongest direction
Vector2D strongestPheromone(Ant* ant) {
    Vector2D direction = {0, 0};
    float max_pheromone = 0.0f;

    for (int dx = -5; dx <= 5; ++dx) {
        for (int dy = -5; dy <= 5; ++dy) {
            int x = (int)ant->position.x + dx;
            int y = (int)ant->position.y + dy;

            if (x >= 0 && x < GRID_WIDTH && y >= 0 && y < GRID_HEIGHT) {
                if (pheromones[x][y].strength > max_pheromone) {
                    max_pheromone = pheromones[x][y].strength;
                    direction.x = dx;
                    direction.y = dy;
                }
            }
        }
    }
    return direction;
}

// Ant behavior based on its state
void updateAntState(Ant* ant, Food foods[], int food_count) {
    switch (ant->state) {
		case TO_FOOD:
			Vector2D pheromone_dir = strongestPheromone(ant);
			
            if (magnitude_2d(pheromone_dir) > 0) {
                ant->desiredDirection = normalize_vector_2d(pheromone_dir);
            } else {
                ant->desiredDirection.x += ((rand() % 3) - 1) * WANDER_STRENGTH;
                ant->desiredDirection.y += ((rand() % 3) - 1) * WANDER_STRENGTH;
                ant->desiredDirection = normalize_vector_2d(ant->desiredDirection);
		  	    //depositPheromones(ant->position.x, ant->position.y, 0);
            }
			int food_indexa = foodNearby(ant, foods, food_count);
			if(food_indexa != -1){
				
	       		Vector2D food_dir = {foods[food_indexa].x - ant->position.x, foods[food_indexa].y - ant->position.y};
	            ant->desiredDirection = normalize_vector_2d(food_dir);


	            if (distance(ant->position.x, ant->position.y, foods[food_indexa].x, foods[food_indexa].y) < FOOD_CAPTURE_RADIUS) {
	                ant->has_food = 1;
	                foods[food_indexa].amount -= 1;
	                if(foods[food_indexa].amount <= 0){
		                foods[food_indexa].exists = 0;     	
	                }
	                ant->state = TO_HOME;
	            }
            
			}
			break;
		case TO_HOME:
	  	    depositPheromones(ant->position.x, ant->position.y, 1, ant->pheromoneStrength);
	  	    ant->pheromoneStrength -= PHEROMONE_DECREASE_RATE;
			ant->desiredDirection.x = 400 - ant->position.x;
    		ant->desiredDirection.y = 300 - ant->position.y;
    		ant->desiredDirection = normalize_vector_2d(ant->desiredDirection);
			if(distance(ant->position.x, ant->position.y, 400, 300) < 2){
    			ant->state = TO_FOOD;
		  	    ant->pheromoneStrength = 100;    			
    		}
			break;
   		
    	
    }
}
void moveAnts(Ant ants[], int ant_count, Food foods[], int food_count) {
    for (int i = 0; i < ant_count; ++i) {
        updateAntState(&ants[i], foods, food_count);

        Vector2D desiredVel = multiply_vector_2d(ants[i].desiredDirection, MAX_SPEED);

  		Vector2D desiredSteeringForce = {(desiredVel.x - ants[i].velocity.x) * STEER_STRENGTH, (desiredVel.y - ants[i].velocity.y) * STEER_STRENGTH};
  		Vector2D clampedDSF = clamp_magnitude_vector_2d(desiredSteeringForce, STEER_STRENGTH);

  		Vector2D acceleration = {clampedDSF.x, clampedDSF.y};
  		acceleration = divide_vector_2d(acceleration, 1);

  		ants[i].velocity = add_vector_2d(ants[i].velocity, acceleration);
  		ants[i].velocity = clamp_magnitude_vector_2d(ants[i].velocity, MAX_SPEED);

      	if (ants[i].position.x < 0) ants[i].position.x = SCREEN_WIDTH - 1;
       	if (ants[i].position.x >= SCREEN_WIDTH) ants[i].position.x = 0;
       	if (ants[i].position.y < 0) ants[i].position.y = SCREEN_HEIGHT - 1;
       	if (ants[i].position.y >= SCREEN_HEIGHT) ants[i].position.y = 0;
  	    ants[i].position.x += ants[i].velocity.x;
        ants[i].position.y += ants[i].velocity.y;
    }
}
void antProcessor(Ant ants[], int ant_count, Food foods[], int food_count){
	for (int i = 0; i < ant_count; ++i) {
		ants[i].desiredDirection.x = ants[i].desiredDirection.x + ((rand() % 3) - 1) * WANDER_STRENGTH;
		ants[i].desiredDirection.y = ants[i].desiredDirection.y + ((rand() % 3) - 1) * WANDER_STRENGTH;

		ants[i].desiredDirection = normalize_vector_2d(ants[i].desiredDirection);
		
		Vector2D desiredVel = multiply_vector_2d(ants[i].desiredDirection, MAX_SPEED);

		Vector2D desiredSteeringForce = {(desiredVel.x - ants[i].velocity.x) * STEER_STRENGTH, (desiredVel.y - ants[i].velocity.y) * STEER_STRENGTH};
		Vector2D clampedDSF = clamp_magnitude_vector_2d(desiredSteeringForce, STEER_STRENGTH);
		Vector2D acceleration = {clampedDSF.x, clampedDSF.y};
		acceleration = divide_vector_2d(acceleration, 1);


	    depositPheromones(ants[i].position.x, ants[i].position.y, 0, 0);

		ants[i].velocity = add_vector_2d(ants[i].velocity, acceleration);
		
		ants[i].velocity = clamp_magnitude_vector_2d(ants[i].velocity, MAX_SPEED);
		
	    
    	if (ants[i].position.x < 0) ants[i].position.x = SCREEN_WIDTH - 1;
       	if (ants[i].position.x >= SCREEN_WIDTH) ants[i].position.x = 0;
       	if (ants[i].position.y < 0) ants[i].position.y = SCREEN_HEIGHT - 1;
       	if (ants[i].position.y >= SCREEN_HEIGHT) ants[i].position.y = 0;
	    ants[i].position.x += ants[i].velocity.x;
        ants[i].position.y += ants[i].velocity.y;
	    
    }
	
}
// Convert world coordinates to screen coordinates based on zoom and camera position
void worldToScreen(int world_x, int world_y, int* screen_x, int* screen_y) {
    *screen_x = (world_x - camera_x) * zoom;
    *screen_y = (world_y - camera_y) * zoom;
}

int main() {
    srand(time(NULL));
    int is_panning = 0;
    int last_mouse_x, last_mouse_y;

	int start_sim = 0;
	
    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        printf("SDL_Init Error: %s\n", SDL_GetError());
        return 1;
    }

    SDL_Window* window = SDL_CreateWindow("Ant Simulation - Zoom & Panning",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        SCREEN_WIDTH, SCREEN_HEIGHT, SDL_WINDOW_SHOWN);
    
    if (window == NULL) {
        printf("SDL_CreateWindow Error: %s\n", SDL_GetError());
        SDL_Quit();
        return 1;
    }

    SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    if (renderer == NULL) {
        SDL_DestroyWindow(window);
        printf("SDL_CreateRenderer Error: %s\n", SDL_GetError());
        SDL_Quit();
        return 1;
    }

    Ant ants[ANT_COUNT];
    Food foods[FOOD_COUNT];
    initAnts(ants, ANT_COUNT);
    initFood(foods, FOOD_COUNT);

    int running = 1;
    SDL_Event e;

    while (running) {
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT) {
                running = 0;
            }
			
            if (e.type == SDL_MOUSEMOTION && is_panning) {
                int mouse_x, mouse_y;
                SDL_GetMouseState(&mouse_x, &mouse_y);
            
                int dx = last_mouse_x - mouse_x;
                int dy = last_mouse_y - mouse_y;
            
                camera_x += dx / zoom;
                camera_y += dy / zoom;
            
                last_mouse_x = mouse_x;
                last_mouse_y = mouse_y;
            }
            if (e.type == SDL_MOUSEBUTTONDOWN) {
                if (e.button.button == SDL_BUTTON_MIDDLE) {
                    is_panning = 1;
                    SDL_GetMouseState(&last_mouse_x, &last_mouse_y);
                }
                if(e.button.button == SDL_BUTTON_LEFT){
                	start_sim = 1;
                }
            }
            
            if (e.type == SDL_MOUSEBUTTONUP) {
                if (e.button.button == SDL_BUTTON_MIDDLE) {
                    is_panning = 0;
                }
            }
            if (e.type == SDL_MOUSEWHEEL) {
                int mouse_x, mouse_y;
                SDL_GetMouseState(&mouse_x, &mouse_y);
            
                int world_mouse_x = camera_x + mouse_x / zoom;
                int world_mouse_y = camera_y + mouse_y / zoom;
            
                if (e.wheel.y > 0) {  // Scroll up
                    zoom *= 1.1f;  // Zoom in
                } else if (e.wheel.y < 0) {  // Scroll down
                    zoom /= 1.1f;  // Zoom out
                }
           
                camera_x = world_mouse_x - mouse_x / zoom;
                camera_y = world_mouse_y - mouse_y / zoom;
            }
        }

        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderClear(renderer);
        if(start_sim){
        	updatePheromones();
   			moveAnts(ants, ANT_COUNT, foods, FOOD_COUNT);
   	        
   	   	    // Render pheromones
   			for (int x = 0; x < GRID_WIDTH; ++x) {
   	           for (int y = 0; y < GRID_HEIGHT; ++y) {
   	           		if (pheromones[x][y].strength > 0.01) {
   	         			int screen_x, screen_y;
   	                   	worldToScreen(x, y, &screen_x, &screen_y);
   	                   	int color_intensity = (int)(pheromones[x][y].strength * 255);
   	                   	if (color_intensity > 255) color_intensity = 255;
						if(pheromones[x][y].type == 0){
	   	                   	SDL_SetRenderDrawColor(renderer, color_intensity, 0, 0, 155);  // Red
						}
						else{
	   	                   	SDL_SetRenderDrawColor(renderer, 0, 0, color_intensity, 155);  // Blue
						}
   	                   	SDL_Rect pheromone_rect = { screen_x, screen_y, 4, 4 };
   	                   	SDL_RenderFillRect(renderer, &pheromone_rect);
   	               }
   	           }
   	       }
   	        
   	
   	        // Render ants
   	        SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
   	        for (int i = 0; i < ANT_COUNT; ++i) {
   	        	
   	            int screen_x, screen_y;
   	            worldToScreen(ants[i].position.x, ants[i].position.y, &screen_x, &screen_y);
   				SDL_Rect ant_rect = { screen_x, screen_y, 4, 4 };
   	            SDL_RenderFillRect(renderer, &ant_rect);
   	            //SDL_RenderDrawPoint(renderer, screen_x, screen_y);
   	        }
   	
   	        // Render food
   	        SDL_SetRenderDrawColor(renderer, 0, 255, 0, 255);
   	        for (int i = 0; i < FOOD_COUNT; ++i) {
   	            if (foods[i].exists) {
   	                int screen_x, screen_y;
   	                worldToScreen(foods[i].x, foods[i].y, &screen_x, &screen_y);
   					SDL_Rect food_rect = { screen_x, screen_y, 4, 4 };
   		            SDL_RenderFillRect(renderer, &food_rect);
   	                //SDL_RenderDrawPoint(renderer, screen_x, screen_y);
   	            }
   	        }
   	
   	        
        }
        SDL_RenderPresent(renderer);
     	SDL_Delay(1000/60);
    }

    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}
