// Make up an ID for ourselves & notify the proxy
var my_id = Math.round(Math.random()*1000000);
print("Hello...");
entity.broadcast("projectileCreated", {id: my_id, px: entity.position.x, pz: entity.position.z});

entity.subscribe("direction_"+my_id, function(msg) {
    // Apply a force in a direction...
    msg = JSON.parse(msg); // It's from the 'outside'...

    // Apply Rodrigues' formula to figure out the direction in which we go
    function cross(u,v) {
	return {
	    x: u.y*v.z - u.z*v.y,
	    y: u.z*v.x - u.x*v.z,
	    z: u.x*v.y - u.y*v.x
	};
    };
    function dot(u,v) {
	return u.x*v.x + u.y*v.y + u.z*v.z;
    };
    var rot = {x: msg.rotation.x, y: msg.rotation.y, z: msg.rotation.z};
    var len = Math.sqrt(rot.x*rot.x + rot.y*rot.y + rot.z*rot.z);
    if (len != 0) {
	rot.x /= len;
	rot.y /= len;
	rot.z /= len;
    }
    var force_v = {
	x: cross(rot,{x:0.0,y:0.0,z:1.0}).x*Math.sin(len) + rot.x*rot.z*(1.0-Math.cos(len)),
	y: cross(rot,{x:0.0,y:0.0,z:1.0}).y*Math.sin(len) + rot.y*rot.z*(1.0-Math.cos(len)),
	z: Math.cos(len) + cross(rot,{x:0.0,y:0.0,z:1.0}).z*Math.sin(len) + rot.z*rot.z*(1.0-Math.cos(len)),
    };

    force_v.x *= 10000.0;
    force_v.y *= 10000.0;
    force_v.z *= 10000.0;
    entity.applyForce(force_v.x, force_v.y, force_v.z, 0.0, 0.0, 0.0);
});

entity.setCallback("update", function(dt) {
    if (entity.position.y < -10.0) {
	entity.broadcast("projectileDestroyed", {id: my_id});
	entity.markForRemoval();
	return;
    }

    entity.broadcast("projectileLocation", {id: my_id, position: entity.position, rotation: entity.rotation});
});
