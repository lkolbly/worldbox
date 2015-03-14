// "entity" is a global variable that's been set for us
//var entity = makeEntity();
print(entity);

var run_num = 0;
var velx = 0.0;

function runpid() {
    var tgtx = 100.0;
    velx += (tgtx-entity.position.x)*0.01
    entity.position.x += velx;
}

function myfunc() {
    entity.position.z += 0.5;
}

entity.position.x = 1.0;
entity.position.y = 2.0;
entity.position.z = 3.0;

entity.setCallback("update", function(dt) {
    runpid();
    print(entity.position.x);

    run_num++;
    if (run_num > 1000)
	entity.markForRemoval();
});

entity.setCallback("update2", function(dt) {
    print("In the callback2! (arg="+dt+")");
    print(" - entity.val: "+entity.val);
});
