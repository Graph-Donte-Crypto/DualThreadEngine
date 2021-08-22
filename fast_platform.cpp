#include <SFML/Graphics/Color.hpp>
#include <SFML/Graphics/RenderWindow.hpp>
#include <SFML/Graphics/Text.hpp>
#include <SFML/System/String.hpp>
#include <SFML/Window/Keyboard.hpp>
#include <UseFull/Math/Equation.hpp>
#include <UseFull/Math/Intersect.hpp>
#include <UseFull/Math/Shape.hpp>
#include <UseFull/Utils/Ok.hpp>
#include <chrono>
#include <cstddef>
#include <cstdio>
#include <functional>
#include <math.h>
#include <ratio>
#include <algorithm>
#include <sstream>
#include <thread>
#include "UseFull/SFMLUp/Mouse.hpp"
#include <UseFull/SFMLUp/Drawer.hpp>
#include <SFML/Window/WindowStyle.hpp>
#include <UseFull/SFMLUp/Event.hpp>
#include <vector>
#include "UseFull/SFMLUp/GUI/FocusTracker.hpp"
#include "UseFull/Utils/Concepts.hpp"

/*
struct PopUp : public sfup::gui::FocusTracker {
	
}
*/

#define waitWhile(value, time) while(value) std::this_thread::sleep_for(std::chrono::milliseconds(time))

bool step_by_step = false;

void waitStepByStep() {

	if (!step_by_step) {
		if (sfup::Event.KeyPressed[sf::Keyboard::P]) {
			step_by_step = true;
			sfup::Event.flush();
		}
	}

	if (step_by_step) {
		while (step_by_step) {
			if (sfup::Event.KeyPressed[sf::Keyboard::P]) {
				step_by_step = false;
				break;
			}
			else if (sfup::Event.KeyPressed[sf::Keyboard::N]) {
				break;
			}
			else 
				std::this_thread::sleep_for(std::chrono::milliseconds(30));
		}
		sfup::Event.flush();
	}
}

using namespace sfup;
using namespace sfup::gui;
using namespace uft;

struct GameObject;
struct GameEnemy;
struct GameBlock;
struct GameBullet;
struct GamePlayer;
struct GameDrop;

struct ClassType {

	const char * ptr = nullptr;
	
	#define Type(type) type = #type
	
	static constexpr const char 
		 * Type(Null) 
		,* Type(Enemy) 
		,* Type(Block) 
		,* Type(Bullet) 
		,* Type(Player) 
		,* Type(Drop) 
		;
	
	#undef Type
	
	ClassType(const char * p) : ptr(p) {} 
	operator const char * () const { return ptr;}
	ClassType & operator = (const char * p) { ptr = p; return *this;}
	bool operator == (const ClassType & type) const { 
		return ptr == type.ptr;};
	bool operator != (const ClassType & type) const { 
		return ptr != type.ptr;};

	bool operator == (const char * type) const {
		return ptr == type;}
	bool operator != (const char * type) const {
		return ptr != type;}
};

struct DrawObject {
	bool destroyed = false;
	std::function<void (Nothing)> lambda;
	void draw() {if (lambda) lambda(Nothing());}
	void destroy();
	DrawObject(utils::CoLambda<void, Nothing> auto lambda);
	~DrawObject();
};

struct CollisionEventStruct {
	GameObject * object = nullptr;
	XY collision_point = {0, 0};
	math::Line<2> collision_line = math::Line<2>({0, 0}, {0, 0});
	enum class Direction {
		Up, 
		Left, 
		Down, 
		Right
	} relative_to_object_collision_direction;
	math::Vector<2> getDirVec() {
		switch(relative_to_object_collision_direction) {
			case Direction::Up:	return {0 , 1};
			case Direction::Left:  return {-1, 0};
			case Direction::Down:  return {0 ,-1};
			case Direction::Right: return {1 , 0};
		}
		printf("CollisionEventStruct::getDirVec::Error FATAL\n");
		exit(1);
	}
	static Direction getDirectionFromIndex(size_t i) {
		switch (i) {
			case 0: return Direction::Up;
			case 1: return Direction::Left;
			case 2: return Direction::Down;
			case 3: return Direction::Right;
			default:
				printf("CollisionEventStruct::getDirectionFromIndex::Error: Illegal Index = %llu\n", i);
				exit(1);
		}
	}
	CollisionEventStruct(GameObject * o, const XY & v, const math::Line<2> & l, Direction dir) {
		object = o; 
		collision_point = v;
		collision_line = l;
		relative_to_object_collision_direction = dir;
	}
	CollisionEventStruct() {
		
	}
};

struct GameObject {
	Codir<2> collision_codir;
	XY speed;
	ClassType type = ClassType::Null;
	bool delete_flag = false;

	static std::vector<GameObject *> * game_objects;

	virtual void draw() = 0;
	virtual void action() = 0;
	virtual ~GameObject() {
	}
	
	void drawCollisionCodir() {
		Drawer.drawCodir(collision_codir, sf::Color::White);
	}

	utils::Ok<CollisionEventStruct> checkCollision(GameObject * obj) { 
		
		if (speed.norm() < 0.0001) return {};

		Codir<2> c = Codir<2> {
			obj->collision_codir.left_up - collision_codir.size(),
			obj->collision_codir.right_down
		} + obj->speed;

		DrawObject draw1([&c](Nothing){
			Drawer.drawCodir(c, sf::Color::Green);
		});

		Codir<2> collision_codir = this->collision_codir;

		DrawObject draw_bullet([&collision_codir](Nothing){
			Drawer.drawCodir(collision_codir, sf::Color::Red);
		});

		waitStepByStep();	   

		if (checkPointInCodir(collision_codir.left_up, c + speed)) {
			std::pair<Ok<XY>, Ok<XY>> l = intersectLineWithCodir(
				Line<2>(
					collision_codir.left_up, 
					collision_codir.left_up + speed
				),
				c
			);

			c -= obj->speed;

			if (l.first.isOk)
				c -= collision_codir.left_up - l.first.value;
		}

		Line<2> delta_line(collision_codir.left_up, collision_codir.left_up + speed);

		DrawObject draw_delta_line([&delta_line](Nothing){
			Drawer.drawLine(delta_line, sf::Color::Green);
		});

		waitStepByStep();

		//Directions: Up, Left, Down, Right
		Line<2> lines[4] = {
			Line<2>(c.left_up, {c.right_down[0], c.left_up[1]}),
			Line<2>({c.right_down[0], c.left_up[1]}, c.right_down),
			Line<2>({c.left_up[0], c.right_down[1]}, c.right_down),
			Line<2>(c.left_up, {c.left_up[0], c.right_down[1]})
		};

		std::sort(lines, lines + std::size(lines) - 1, [&collision_codir] (const Line<2> & line1, const Line<2> & line2) {
			return 
				distanceBeetweenPointAndLineMin(collision_codir.left_up, line1) >
				distanceBeetweenPointAndLineMin(collision_codir.left_up, line2);
		}); 

		Ok<Vector<2>> oks[4] = {
			intersectLineWithLine2D(delta_line, lines[0]),
			intersectLineWithLine2D(delta_line, lines[1]),
			intersectLineWithLine2D(delta_line, lines[2]),
			intersectLineWithLine2D(delta_line, lines[3])
		};

		double min_dist = 0;
		size_t index = 0;
		bool collision_was = false;
		
		size_t j = 0;
		for (; j < 4; j++) {
			if (oks[j].isOk) {
				index = j; 
				min_dist = collision_codir.left_up.distanceTo(oks[j].value);
				collision_was = true;
				break;
			}
		}
		for (; j < 4; j++) {
			if (oks[j].isOk) {
				double dis = collision_codir.left_up.distanceTo(oks[j].value);
				if (dis < min_dist) {
					index = j; 
					min_dist = dis;
				}
			}
		}

		if (collision_was) {
			return CollisionEventStruct(
				obj,
				oks[index].value,
				lines[index],
				CollisionEventStruct::getDirectionFromIndex(index)
			);
		}
		else return {};
	}
};


struct GlobalStruct {
	
	std::vector<GameObject *> game_objects;
	std::vector<GameObject *> game_objects_next_turn;
	std::vector<DrawObject *> draw_objects;

	const char * window_name = "Fast Platform";
	sf::RenderWindow window;

	bool draw_pause = false;
	bool draw_pause_active = false;

	bool draw_stop = false;
	bool draw_stop_active = false;
	
	void drawPause() {
		using namespace std;
		draw_pause = true;
		waitWhile(!draw_pause_active, 10);
	}
	
	void drawUnpause() {
		draw_pause = false;
	}

	size_t limit_fps = 50;
	
	sf::Clock clock;
	double current_time;
	double current_fps;
	
	size_t window_width = 800;
	size_t window_height = 600;
	
	size_t video_mode = sf::Style::Close;
	
	void calcFps() {
		current_time = clock.restart().asSeconds();
		current_fps = (double)1.0 / current_time;
	}
	
	void createWindow(size_t mode) {
		window.create(sf::VideoMode(window_width, window_height), window_name, mode);
		window.setFramerateLimit(limit_fps);
		window.setVerticalSyncEnabled(true);
		Drawer.target = &window;
		Mouse.window = &window;
	}
	
	GlobalStruct() {
		GameObject::game_objects = &game_objects;
		//createWindow(sf::Style::Fullscreen);
		createWindow(sf::Style::Close);
	}	
} 
Global;

struct MainViewStruct {
	sf::View view;
	XY new_center, curent_center;
	size_t coef = 3;
	MainViewStruct() {
		curent_center = {0, 0};
		new_center = curent_center;
		view.reset(sf::FloatRect(0.0f, 0.0f, Global.window_width, Global.window_height));
	};

	void step() {
		curent_center += (new_center - curent_center) / coef;
		view.setCenter(curent_center[0], curent_center[1]);
		Global.window.setView(view);
	}
} MainView;

std::vector<GameObject *> * GameObject::game_objects = nullptr;

struct GameBlock : public GameObject, public FocusTracker {
	GameBlock() : GameObject(), FocusTracker(collision_codir) {
		type = ClassType::Block;
		color = sf::Color(191, 191, 191, 255);
	}

	sf::Color color;

	void actionFocused() override {
		color = sf::Color::Green;
	}

	void actionNotFocused() override {
		color = sf::Color(191, 191, 191, 255);
	}

	void action() override {
		FocusTracker::checkFocus(Mouse.inWorld);
	}

	void draw() override {
		Drawer.drawCodirFilled(collision_codir, sf::Color::White, color);
	}
};

struct GameBullet : public GameObject {
	size_t time_to_destroy = 12;
	double r = 2.5;
	double v = 30;

	GameBullet(const XY & ort) : GameObject() {
		type = ClassType::Bullet;
		collision_codir = Codir<2>({0, 0}, {r, r});
		speed = ort * v;
	}

	void action() override {

		for (auto & object : *game_objects) {
			if (object->type == ClassType::Enemy) {
				auto ces = checkCollision(object);
				if (ces.isOk) {
					object->speed += speed;
					time_to_destroy = 0;
					break;
				}
			}
			else if (object->type == ClassType::Block) {
				auto ok = checkCollision(object);
				if (ok.isOk) {
					auto & ces = ok.value;

					math::EquationHyperplane<2> eh(ces.collision_point, ces.getDirVec());
					
					XY projection = math::projectionPointOnEquationHyperplane(collision_codir.left_up, eh);

					DrawObject draw1([&projection](Nothing){
							Drawer.drawCircle(Sphere<2>(projection, 2), sf::Color::Blue);
					});
					waitStepByStep();

					XY projection_fin = ces.collision_point + (ces.collision_point - projection);

					DrawObject draw2([&projection_fin](Nothing){
							Drawer.drawCircle(Sphere<2>(projection_fin, 3), sf::Color::Blue);
					});
					waitStepByStep();

					XY projection_start = projection + (projection - collision_codir.left_up);

					DrawObject draw3([&projection_start](Nothing){
							Drawer.drawCircle(Sphere<2>(projection_start, 4), sf::Color::Blue);
					});
					waitStepByStep();

					XY new_speed = projection_fin - projection_start;

					DrawObject draw4([&new_speed, this](Nothing){
							Drawer.drawLine(math::Line<2>(collision_codir.left_up, collision_codir.left_up + new_speed), sf::Color::Blue);
					});
					waitStepByStep();

					double speed_total = speed.norm();
					double speed_len = (ces.collision_point - collision_codir.left_up - speed).norm();

					XY result_point = ces.collision_point + new_speed.ort() * (speed_total - speed_len);
					DrawObject draw5([&result_point](Nothing){
							Drawer.drawCircle(Sphere<2>(result_point, 5), sf::Color::Red);
					});
					waitStepByStep();

					speed = new_speed;

					collision_codir += result_point - collision_codir.left_up;
					collision_codir -= speed;

					waitStepByStep();
				}
			}
		}
		collision_codir += speed;
		if (time_to_destroy > 0) time_to_destroy -= 1;
		else delete_flag = true;
	}

	void draw() override {
		Drawer.drawCircle(
			Sphere<2>(collision_codir.left_up - speed, r / 4),
			sf::Color(255, 255, 255, 63)
		);
		Drawer.drawCircle(
			Sphere<2>(collision_codir.left_up - speed * 2.0 / 3, r / 2),
			sf::Color(255, 255, 255, 127)
		);
		Drawer.drawCircle(
			Sphere<2>(collision_codir.left_up - speed / 3, r * 3 / 4),
			sf::Color(255, 255, 255, 191)
		);
		Drawer.drawCircle(
			Sphere<2>(collision_codir.left_up, r), 
			sf::Color::White
		);
	}
};

struct GamePlayer : public GameObject {
	double v = 5;
	double max_v = 10;

	size_t reload_time_limit = 10;
	size_t reload_time_current = 0;

	GamePlayer() : GameObject() {
		type = ClassType::Player;
	}

	GameBlock * block = nullptr;

	void action() override {
		//Speed calculation
		if (Event.KeyPressing[sf::Keyboard::W]) 
			speed -= {0, v};
		if (Event.KeyPressing[sf::Keyboard::A]) 
			speed -= {v, 0};
		if (Event.KeyPressing[sf::Keyboard::S]) 
			speed += {0, v};
		if (Event.KeyPressing[sf::Keyboard::D]) 
			speed += {v, 0};

		//speed += {0, 0.1};

		if (Event.e[ev::evMouseButtonPressedRight]) {
			block = new GameBlock();
			Global.game_objects_next_turn.push_back(block);
			block->collision_codir.left_up = Mouse.inWorld;
			block->collision_codir.right_down = Mouse.inWorld;
			block->codir_focus = block->collision_codir;
		}
		if (Event.e[ev::evMouseButtonPressingRight]) {
			if (Mouse.inWorld > block->collision_codir.left_up)
				block->collision_codir.right_down = Mouse.inWorld;
			else 
				block->collision_codir.left_up = Mouse.inWorld;

			block->codir_focus = block->collision_codir;
		}
		if (Event.e[ev::evMouseButtonReleasedRight]) {
			block = nullptr;
		}

		for (auto & object : *game_objects) {
			if (object->type == ClassType::Block) {
				auto ok = checkCollision(object);
				if (ok.isOk) {
					auto & ces = ok.value;

					auto end_point = math::projectionPointOnEquationLine(
							collision_codir.left_up + speed, 
							EquationLine(ces.collision_line)
					);

					if (!end_point.isOk) exit(1);

					speed = end_point.value - collision_codir.left_up - ces.getDirVec() * 0.001;
				}
			}
		}

		//Bullet creation
		if (reload_time_current > 0) reload_time_current -= 1;
		else {
			if (Event.e[evMouseButtonPressingLeft]) {
				GameObject * bullet = new GameBullet(
					(Mouse.inWorld - collision_codir.center()).ort()
				);
				bullet->collision_codir += collision_codir.center();
				Global.game_objects_next_turn.push_back(bullet);
				reload_time_current = reload_time_limit;
				MainView.curent_center -= bullet->speed.ort() * 8;
			}
		}

		speed *= (1 - (v / (max_v + v)));
		collision_codir += speed;
	}
	void draw() override {
		drawCollisionCodir();
	}
};

struct GameEnemy : public GameObject {

	GamePlayer * target = nullptr;

	double find_distance = 100;
	double forget_distance = 300;

	GameEnemy() : GameObject() {
		type = ClassType::Enemy;
		collision_codir = Codir<2>({0, 0}, {12, 12});
	}

	void action() override {
		if (!target) {
			for (auto & obj : *game_objects) {
				if (obj->type == ClassType::Player) {
					double distance2 = obj->collision_codir
						.left_up
						.distanceSquaredTo(collision_codir.left_up);

					if (distance2 < find_distance * find_distance) {
						target = (GamePlayer *)obj;
						break;
					}
				}
			}
		}
		else {
			double distance2 = target->collision_codir
				.left_up
				.distanceSquaredTo(collision_codir.left_up);

			if (distance2 > forget_distance * forget_distance)
				target = nullptr;
			else
				speed += (target->collision_codir.left_up - collision_codir.left_up).ort();
		}

		collision_codir += speed;
		speed *= 0.8;
	}

	void draw() override {
		drawCollisionCodir();
	}
};



struct GameDrop : public GameObject {
	GameDrop() : GameObject() {
		type = ClassType::Drop;
	}
};



struct StringBuildStruct {
	std::ostringstream buffer;
	template<typename Type>
	StringBuildStruct & operator << (const Type & t) {
		buffer << t;
		return *this;
	}
	std::string toString() {
		std::string s = buffer.str();
		buffer.str("");
		return s;
	}
}
StringBuild;



DrawObject::DrawObject(utils::CoLambda<void, Nothing> auto lambda) {
	Global.draw_objects.push_back(this);
	this->lambda = lambda;
}

void DrawObject::destroy() {
	if (!destroyed) {
		for (size_t i = 0; i < Global.draw_objects.size(); i++) {
			if (Global.draw_objects[i] == this) {
				Global.draw_objects.erase(Global.draw_objects.begin() + i);
				break;
			}
		}
		destroyed = true;
	}
}

DrawObject::~DrawObject() {
	destroy();
}

void loadSources() {
	Fonts.load("source/f/verdana.ttf", "verdana");
	Fonts.load("source/f/UbuntuMono-R.ttf" ,"UbuntuMono-R" );
	Fonts.load("source/f/UbuntuMono-RI.ttf","UbuntuMono-RI");
	Fonts.load("source/f/UbuntuMono-B.ttf" ,"UbuntuMono-B" );
	Fonts.load("source/f/UbuntuMono-BI.ttf","UbuntuMono-BI");
}

void drawCycle() {
	using namespace std;

	sf::RenderWindow & window = Global.window;

	while (!Global.draw_stop && window.isOpen()) {
		Event.load(window);

		Mouse.getPosition(window);

		while (Global.draw_pause) {
			Global.draw_pause_active = true;
			this_thread::sleep_for(chrono::milliseconds(10));
		}
		Global.draw_pause_active = false;
		
		MainView.step();

		window.clear(sf::Color::Black);

		for (auto & object : Global.game_objects)
			object->draw();
		for (auto & object : Global.draw_objects) 
			object->draw();

		window.display();
	}
	printf("Draw Stop\n");
}

void actionCycle() {
	sf::RenderWindow & window = Global.window;
	std::vector<GameObject *> & game_objects = Global.game_objects;

	size_t milliseconds_per_frame = 20;

	GamePlayer * player = new GamePlayer();
	player->collision_codir = Codir<2>({0, 0}, {10, 10});
	game_objects.push_back(player);

	GameBlock * block_test = new GameBlock();
	block_test->collision_codir = math::Codir<2>({-100, 50}, {100, 100});
	block_test->codir_focus = block_test->collision_codir;
	game_objects.push_back(block_test);

	while (window.isOpen()) {
		//Delete objects
		for (size_t i = 0; i < game_objects.size(); i++) {
			GameObject * object = game_objects[i];
			if (object->delete_flag) {
				//Global.drawPause(); //if game unexpected crush - activate pause sync
				game_objects.erase(game_objects.begin() + i);
				delete object;
				i -= 1;
			}
		}
		//Global.drawUnpause();

		//Add new objects
		for (size_t i = 0; i < Global.game_objects_next_turn.size(); i++) 
			game_objects.push_back(Global.game_objects_next_turn[i]);
		Global.game_objects_next_turn.clear();

		/*
		//Action Pause if Z pressed
		if (Event.KeyPressed[sf::Keyboard::Z]) {
			Event.flush();
			waitWhile(!Event.KeyPressed[sf::Keyboard::Z], 30);
		}
		*/

		if (Event.KeyPressed[sf::Keyboard::Q]) {
			GameEnemy * enemy = new GameEnemy();
			Global.game_objects_next_turn.push_back(enemy);
		}

		//action for all objects
		for (auto & object : Global.game_objects)
			object->action();

		//Recalculate view center
		MainView.new_center = (player->collision_codir.center() * 2 + Mouse.inWorld) / 3;
	 
		FocusTracker::focus = FocusTracker::focus_next_turn;
		FocusTracker::focus_next_turn = nullptr;

		//Flush events
		Event.flush();

		std::this_thread::sleep_for(std::chrono::milliseconds(milliseconds_per_frame));
	}
}

int main() {
	Global.createWindow(sf::Style::Default);
	loadSources();
	Fonts.setFontToText("UbuntuMono-R", Drawer.text);
	std::thread thread_action(actionCycle);
	std::thread thread_draw(drawCycle);
	thread_action.detach();
	thread_draw.join();
}
