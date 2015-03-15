// "entity" is a global variable that's been set for us

var run_num = 0;

entity.setCallback("update", function(dt) {
    run_num++;
    if (run_num > 1000) {
	entity.markForRemoval();
    }
});

entity.subscribe("test_msgs", function(msg) {
    print("Received message: "+JSON.stringify(msg));
});
