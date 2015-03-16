var socket = new WebSocket("ws://localhost:8888/ws");

var position = {};

socket.onmessage = function(event) {
    var obj = JSON.parse(event.data);
    position = obj;
    $("#cart-position").html(event.data);
};

$("#disconnect").click(function() {
    socket.close();
});

var controls = {throttle: 0.0, steer: 0.0};

function sendControls() {
    socket.send(JSON.stringify({type: "controls", controls: controls}));
};

$("body").keypress(function(event) {
    //console.log(event);
    if (event.keyCode === 119) { // W
	controls.throttle += 0.1;
    } else if (event.keyCode === 97) { // A
	controls.steer += 0.1;
    } else if (event.keyCode === 115) { // S
	controls.throttle -= 0.1;
    } else if (event.keyCode === 100) { // D
	controls.steer -= 0.1;
    }
    sendControls();
});

$(function() {
    // Largely from stemkoski's great 3JS examples (incl. checkerboard)
    var scene = new THREE.Scene();

    var camera = new THREE.PerspectiveCamera(45.0, 640/480, 0.1, 1000.0);
    scene.add(camera);
    camera.position.set(0,15,40);
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
    var floorGeo = new THREE.PlaneGeometry(250, 250, 1, 1);
    var floor = new THREE.Mesh(floorGeo, floorMat);
    floor.position.y = -0.5;
    floor.rotation.x = Math.PI / 2.0;
    scene.add(floor);

    function animate() {
	requestAnimationFrame(animate);
	cube.position.x = position.position.x;
	cube.position.y = position.position.y;
	cube.position.z = position.position.z;
	renderer.render(scene, camera);
    }
    requestAnimationFrame(animate);
});
