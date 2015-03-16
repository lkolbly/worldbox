// Make up an ID for ourselves & notify the proxy
var my_id = Math.round(Math.random()*1000000);
print("Hello...");
entity.broadcast("kartCreated", {id: my_id});

var controls = {throttle: 0, steer: 0};

entity.subscribe("controls_"+my_id, function(msg) {
    msg = JSON.parse(msg); // We have to parse it since it's from the 'outside'
    if (msg.hasOwnProperty("throttle")) controls.throttle = msg.throttle;
    if (msg.hasOwnProperty("steer")) controls.steer = msg.steer;
});

entity.subscribe("kill_"+my_id, function(msg) {
    entity.markForRemoval();
});

entity.setCallback("update", function(dt) {
    // Apply forces according to throttle
    // +Z is forward. +Y is up. +X is left.
    entity.applyForce(0.0,0.0,controls.throttle, 0.0,0.0,0.0);

    // Apply steer force
    entity.applyForce(controls.steer,0.0,0.0, 0.0,0.0,1.0);
    entity.applyForce(-controls.steer,0.0,0.0, 0.0,0.0,-1.0);

    // Update the player with our location
    entity.broadcast("kartLocation", {id: my_id, position: entity.position, rotation: entity.rotation});
});
