var controls = {throttle: 0, steer: 0};

entity.subscribe("controls_"+entity.id, function(msg) {
    msg = JSON.parse(msg); // We have to parse it since it's from the 'outside'
    if (msg.hasOwnProperty("throttle")) controls.throttle = msg.throttle;
    if (msg.hasOwnProperty("steer")) controls.steer = msg.steer;

    if (msg.hasOwnProperty("shoot")) {
	if (msg.shoot === true) {
	    var p = {x: entity.position.x, y: entity.position.y, z: entity.position.z};
	    entity.spawnEntity("examples/karts/game_assets/projectile.json", JSON.stringify({rot: {x: entity.rotation.x, y: entity.rotation.y, z: entity.rotation.z}}), p.x,p.y+1.5,p.z, 0.0,0.0,0.0);
	    msg.shoot = false;
	}
    }
});

entity.setCallback("update", function(dt) {
    if (entity.position.y < -10.0) {
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

    // Apply an air friction
    var FRIC = 1.0;
    var vel = {
	x: -entity.velocity.x*FRIC,
	y: -entity.velocity.y*FRIC,
	z: -entity.velocity.z*FRIC
    };
    entity.applyForce(vel.x, vel.y, vel.z, 0.0,0.0,0.0);
});
