// "entity" is a global variable that's been set for us
//var entity = makeEntity();
print(entity);

var run_num = 0;

entity.position.x = -1.0;
entity.position.y = 2.0;
entity.position.z = 3.0;

entity.setCallback("update", function(dt) {
    print("test2's xpos is "+entity.position.x);

    run_num++;
    if (run_num > 1010)
	entity.markForRemoval();
});
