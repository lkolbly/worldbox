// "entity" is a global variable that's been set for us
//var entity = makeEntity();
print(entity);

var run_num = 0;

/*entity.position.x = -1.0;
entity.position.y = 2.0;
entity.position.z = 3.0;*/

entity.setCallback("update", function(dt) {
    if (run_num > 10) {
	print("Accelerating");
	entity.applyForce(3.0,0.0,0.0, 0.0,0.0,0.0);
    }
    print("test2's x,y,z (at "+run_num+") is "+entity.position.x+"\t"+entity.position.y+"\t"+entity.position.z);
    print(" - Rotation: "+entity.rotation);
    print(" - Velocity: "+entity.velocity);
    print(" - Rotvel: "+entity.rotvel);

    run_num++;
    if (run_num > 1010)
	entity.markForRemoval();
});
