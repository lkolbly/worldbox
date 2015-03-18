var socket = new WebSocket("ws://"+window.location.hostname+":8888/ws");

var position = {};
var other_positions = {};
var proj_positions = {};

socket.onmessage = function(event) {
    var obj = JSON.parse(event.data);
    if (obj.type === "my_loc") {
	position = obj;
	//console.log(obj);
	$("#cart-position").html(event.data);
    } else if (obj.type === "other_loc" && obj.id !== position.id) {
	other_positions[obj.id] = obj;
    } else if (obj.type === "proj_loc") {
	proj_positions[obj.id] = obj;
    }
};

$("#disconnect").click(function() {
    socket.close();
});

var controls = {throttle: 0.0, steer: 0.0, shoot: false};

function sendControls() {
    socket.send(JSON.stringify({type: "controls", controls: controls}));
    controls.shoot = false;
};

var down_keys = {};
function updateControls() {
    controls.throttle = 0.0;
    controls.steer = 0.0;
    controls.shoot = false;
    if (down_keys[87] === true) { // W
	controls.throttle += 20.0;
    }
    if (down_keys[65] === true) { // A
	controls.steer += 0.4;
    }
    if (down_keys[83] === true) { // S
	controls.throttle += -20.0;
    }
    if (down_keys[68] === true) { // D
	controls.steer += -0.4;
    }
    if (down_keys[32] === true) { // Space
	// Shoot something...
	controls.shoot = true;
    }
}

$("body").keydown(function(event) {
    if (down_keys[event.keyCode] === true) return;
    console.log(event);
    down_keys[event.keyCode] = true;
    updateControls();
    sendControls();
});

$("body").keyup(function(event) {
    down_keys[event.keyCode] = false;
    updateControls();
    sendControls();
});

var other_gfx = {};
var proj_gfx = {};

$(function() {
    // Largely from stemkoski's great 3JS examples (incl. checkerboard)
    var scene = new THREE.Scene();

    var camera = new THREE.PerspectiveCamera(45.0, 640/480, 0.1, 1000.0);
    scene.add(camera);
    //camera.position.set(0,15,-40);
    camera.lookAt(scene.position);

    var renderer = new THREE.WebGLRenderer();
    renderer.setSize(640,480);
    document.getElementById('3js').appendChild(renderer.domElement);

    var boxGeometry = new THREE.CubeGeometry(1.0,1.0,2.0);
    var material = new THREE.MeshBasicMaterial({color: 0xff8800});
    var cube = new THREE.Mesh(boxGeometry, material);
    scene.add(cube);

    // The floor...
    var floorTexture = new THREE.ImageUtils.loadTexture('images/checkerboard.jpg');
    floorTexture.wrapS = floorTexture.wrapT = THREE.RepeatWrapping;
    floorTexture.repeat.set(10,10);
    var floorMat = new THREE.MeshBasicMaterial({map:floorTexture, side: THREE.DoubleSide});
    var floorGeo = new THREE.PlaneGeometry(100, 100, 1, 1);
    var floor = new THREE.Mesh(floorGeo, floorMat);
    floor.position.y = 0.0;
    floor.rotation.x = Math.PI / 2.0;
    scene.add(floor);

    floorGeo = new THREE.SphereGeometry(80.0, 30,30);
    floor = new THREE.Mesh(floorGeo, floorMat);
    floor.position.y = -75.0;
    scene.add(floor);

    function animate() {
	requestAnimationFrame(animate);
	cube.position.x = position.position.x;
	cube.position.y = position.position.y+0.5;
	cube.position.z = position.position.z;

	var rotation = {x: position.rotation.x, y: position.rotation.y, z: position.rotation.z};
	var len = Math.sqrt(rotation.x*rotation.x + rotation.y*rotation.y + rotation.z*rotation.z);
	rotation.x /= len;
	rotation.y /= len;
	rotation.z /= len;
	cube.quaternion.setFromAxisAngle(new THREE.Vector3(rotation.x,rotation.y,rotation.z), len);

	camera.position.copy(cube.position);
	camera.quaternion.copy(cube.quaternion);
	camera.updateMatrix();
	camera.updateMatrixWorld();
	camera.translateOnAxis(new THREE.Vector3(0.0,10.0,-20.0), 1.0);
	camera.lookAt(cube.position);

	// Update the others...
	for (var k in other_positions) {
	    if (other_positions.hasOwnProperty(k)) {
		if (!other_gfx.hasOwnProperty(k)) {
		    // Setup an other cube...
		    var boxGeometry = new THREE.CubeGeometry(1.0,1.0,2.0);
		    var material = new THREE.MeshBasicMaterial({color: 0xff3300});
		    var c = new THREE.Mesh(boxGeometry, material);
		    scene.add(c);
		    other_gfx[k] = c;
		}

		other_gfx[k].position.x = other_positions[k].position.x;
		other_gfx[k].position.y = other_positions[k].position.y+0.5;
		other_gfx[k].position.z = other_positions[k].position.z;

		var rotation = {x: other_positions[k].rotation.x, y: other_positions[k].rotation.y, z: other_positions[k].rotation.z};
		var len = Math.sqrt(rotation.x*rotation.x + rotation.y*rotation.y + rotation.z*rotation.z);
		rotation.x /= len;
		rotation.y /= len;
		rotation.z /= len;
		other_gfx[k].quaternion.setFromAxisAngle(new THREE.Vector3(rotation.x,rotation.y,rotation.z), len);
	    }
	}

	for (var k in proj_positions) {
	    if (proj_positions.hasOwnProperty(k)) {
		if (!proj_gfx.hasOwnProperty(k)) {
		    // Setup an other cube...
		    var boxGeometry = new THREE.SphereGeometry(0.3);
		    var material = new THREE.MeshBasicMaterial({color: 0x008800});
		    var c = new THREE.Mesh(boxGeometry, material);
		    scene.add(c);
		    proj_gfx[k] = c;
		}

		proj_gfx[k].position.x = proj_positions[k].position.x;
		proj_gfx[k].position.y = proj_positions[k].position.y;
		proj_gfx[k].position.z = proj_positions[k].position.z;
	    }
	}

	renderer.render(scene, camera);
    }
    requestAnimationFrame(animate);
});
