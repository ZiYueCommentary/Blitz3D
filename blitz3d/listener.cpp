#include "std.h"
#include "listener.h"

Listener::Listener(float roll, float dopp, float dist) {
	bbSet3dListenerConfig(roll, dopp, dist);
	renderListener();
}

Listener::Listener(const Listener& t) :
	Object(t) {
}

Listener::~Listener() {
	bbSet3dListener(0, 0, 0, 0, 0, 1, 0, 1, 0, 0, 0, 0);
}

void Listener::renderListener() {
	const Vector& pos = getWorldTform().v;
	const Vector& vel = getVelocity();
	const Vector& forward = getWorldTform().m.k.normalized();
	const Vector& up = getWorldTform().m.j.normalized();

	bbSet3dListener(pos.x, pos.y, pos.z, forward.x, forward.y, forward.z, up.x, up.y, up.z, vel.x, vel.y, vel.z);
}