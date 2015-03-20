msg = JSON.parse(config);

// Apply a force in a direction...

// Apply Rodrigues' formula to figure out the direction in which we go
var u = new vec3({x: 0.0, y: 0.0, z: 1.0});
var v = new vec3(msg.rot);
var rot_amt = v.length();
var axis = v.normalize();
var force_v = u.rotateAroundAxis(axis, rot_amt);

force_v.x *= 10000.0;
force_v.y *= 10000.0;
force_v.z *= 10000.0;
entity.applyForce(force_v.x, force_v.y, force_v.z, 0.0, 0.0, 0.0);

entity.setCallback("update", function(dt) {
    if (entity.position.y < -10.0) {
	entity.broadcast("projectileDestroyed", {id: entity.id});
	entity.markForRemoval();
	return;
    }
});
