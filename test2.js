// "entity" is a global variable that's been set for us

var run_num = 0;

var velocity_pid = createBinaryController(-1.0, 1.0, 0.01);
velocity_pid.setTarget(1.0);

var position_pid = createPIDController(1.0, 0.0, 1.0);
position_pid.setTarget(100.0);

entity.setCallback("update", function(dt) {
    if (run_num > 10) {
	print("Accelerating");

	var tgtvel = position_pid.update(dt, entity.position.x);
	velocity_pid.setTarget(tgtvel);

	var fx = velocity_pid.update(dt, entity.velocity.x);
	entity.applyForce(fx,0.0,0.0, 0.0,0.0,0.0);
    }
    print("test2's x,y,z (at "+run_num+") is "+entity.position.x+"\t"+entity.position.y+"\t"+entity.position.z);
    print(" - Rotation: "+entity.rotation);
    print(" - Velocity: "+entity.velocity);
    print(" - Rotvel: "+entity.rotvel);

    entity.broadcast("test_msgs", {msg: "test", position_x: entity.position.x});

    run_num++;
    if (run_num > 1010)
	entity.markForRemoval();
});
