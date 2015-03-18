// Make up an ID for ourselves & notify the proxy
var my_id = Math.round(Math.random()*1000000);
print("Hello...");
entity.broadcast("kartCreated", {id: my_id});

var controls = {throttle: 0, steer: 0};

entity.subscribe("controls_"+my_id, function(msg) {
    msg = JSON.parse(msg); // We have to parse it since it's from the 'outside'
    if (msg.hasOwnProperty("throttle")) controls.throttle = msg.throttle;
    if (msg.hasOwnProperty("steer")) controls.steer = msg.steer;

    if (msg.hasOwnProperty("shoot")) {
	if (msg.shoot === true) {
	    var p = {x: entity.position.x, y: entity.position.y, z: entity.position.z};
	    //var p = {x: 0.0, y: 0.0, z: 0.0};
	    entity.broadcast("requestProjectile", {position: p, rotation: {x: entity.rotation.x, y: entity.rotation.y, z: entity.rotation.z}});
	    msg.shoot = false;
	}
    }
});

entity.subscribe("kill_"+my_id, function(msg) {
    entity.markForRemoval();
});

entity.setCallback("update", function(dt) {
    if (entity.position.y < -10.0) {
	entity.broadcast("kartDestroyed", {id: my_id});
	entity.markForRemoval();
	return;
    }

    // Apply forces according to throttle
    // +Z is forward. +Y is up. +X is left.
    entity.applyForce(0.0,0.0,controls.throttle, 0.0,0.0,0.0);

    // Apply steer force
    if (entity.velocity.z > 0.0) {
	entity.applyForce(controls.steer,0.0,0.0, 0.0,0.0,1.0);
	entity.applyForce(-controls.steer,0.0,0.0, 0.0,0.0,-1.0);
    } else {
	// Reverse if going backwards (hackish much?)
	entity.applyForce(-controls.steer,0.0,0.0, 0.0,0.0,1.0);
	entity.applyForce(controls.steer,0.0,0.0, 0.0,0.0,-1.0);
    }

    // Determine our orientation
    var rotation = {x: entity.rotation.x, y: entity.rotation.y, z: entity.rotation.z};
    var len = Math.sqrt(rotation.x*rotation.x + rotation.y*rotation.y + rotation.z*rotation.z);
    if (len != 0) {
	rotation.x /= len;
	rotation.y /= len;
	rotation.z /= len;
    }

    // Apply an air friction
    var FRIC = 1.0;
    var vel = {
	x: -entity.velocity.x*FRIC,
	y: -entity.velocity.y*FRIC,
	z: -entity.velocity.z*FRIC
    };
    entity.applyForce(vel.x, vel.y, vel.z, 0.0,0.0,0.0);

    // Update the player with our location
    entity.broadcast("kartLocation", {id: my_id, position: entity.position, rotation: entity.rotation});
});
