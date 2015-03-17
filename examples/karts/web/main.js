var socket = new WebSocket("ws://"+window.location.hostname+":8888/ws");

var position = {};
var other_positions = {};

socket.onmessage = function(event) {
    var obj = JSON.parse(event.data);
    if (obj.type === "my_loc") {
	position = obj;
	//console.log(obj);
	$("#cart-position").html(event.data);
    } else if (obj.type === "other_loc" && obj.id !== position.id) {
	other_positions[obj.id] = obj
    }
};

$("#disconnect").click(function() {
    socket.close();
});

var controls = {throttle: 0.0, steer: 0.0};

function sendControls() {
    socket.send(JSON.stringify({type: "controls", controls: controls}));
};

var down_keys = {};
$("body").keydown(function(event) {
    if (down_keys[event.keyCode] === true) return;
    console.log(event);
    if (event.keyCode === 87) { // W
	controls.throttle = 20.0;
    } else if (event.keyCode === 65) { // A
	controls.steer = 0.3;
    } else if (event.keyCode === 83) { // S
	controls.throttle = -20.0;
    } else if (event.keyCode === 68) { // D
	controls.steer = -0.3;
    }
    down_keys[event.keyCode] = true;
    sendControls();
});

$("body").keyup(function(event) {
    if (event.keyCode === 87 || event.keyCode === 83) {
	controls.throttle = 0.0;
    }
    if (event.keyCode === 65 || event.keyCode === 68) {
	controls.steer = 0.0;
    }
    down_keys[event.keyCode] = false;
    sendControls();
});

var other_gfx = {};

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
    floor.position.y = -0.5;
    floor.rotation.x = Math.PI / 2.0;
    scene.add(floor);

    function animate() {
	requestAnimationFrame(animate);
	cube.position.x = position.position.x;
	cube.position.y = position.position.y;
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
		other_gfx[k].position.y = other_positions[k].position.y;
		other_gfx[k].position.z = other_positions[k].position.z;

		var rotation = {x: other_positions[k].rotation.x, y: other_positions[k].rotation.y, z: other_positions[k].rotation.z};
		var len = Math.sqrt(rotation.x*rotation.x + rotation.y*rotation.y + rotation.z*rotation.z);
		rotation.x /= len;
		rotation.y /= len;
		rotation.z /= len;
		other_gfx[k].quaternion.setFromAxisAngle(new THREE.Vector3(rotation.x,rotation.y,rotation.z), len);
	    }
	}

	renderer.render(scene, camera);
    }
    requestAnimationFrame(animate);
});
