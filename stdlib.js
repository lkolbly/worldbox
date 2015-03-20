/**
 * stdlib.js - A standard library of functions for the entities. This file
 * is implicitely included at the top of every imported entity JS file.
 */

var createBinaryController = function(min, max, deadband) {
    return {
	min: min,
	max: max,
	deadband2: deadband*deadband,
	target: 0.0,
	setTarget: function(tgt) { this.target = tgt; },

	update: function(dt, val) {
	    var e = val - this.target;
	    if (e*e < this.deadband2) return 0.0;
	    if (val < this.target) return this.max;
	    return this.min;
	}
    };
};

var createPIDController = function(kp, ki, kd) {
    return {
	kp: kp,
	ki: ki,
	kd: kd,
	target: 0.0,
	setTarget: function(tgt) { this.target = tgt; },

	integral: 0.0,
	last_val: 0.0,

	update: function(dt, val) {
	    var e = this.target - val;

	    // Proportional term
	    var prop = this.kp * e;

	    // Integral term
	    this.integral += e;
	    var integral = this.integral * this.ki;

	    // Derivative term
	    var de_dt = (val - this.last_val) / dt;
	    var derivative = -de_dt * this.kd;

	    this.last_val = val;
	    return prop + integral + derivative;
	}
    };
};

var vec3 = function(v) {
    this.x = v.x;
    this.y = v.y;
    this.z = v.z;
};

vec3.prototype.cross = function(other) {
    return new vec3({
	x: this.y*other.z - this.z*other.y,
	y: this.z*other.x - this.x*other.z,
	z: this.x*other.y - this.y*other.x
    });
};

vec3.prototype.dot = function(other) {
    return this.x*other.x + this.y*other.y + this.z*other.z;
};

vec3.prototype.scale = function(s) {
    return new vec3({
	x: this.x*s,
	y: this.y*s,
	z: this.z*s
    });
};

vec3.prototype.length = function() {
    return Math.sqrt(this.x*this.x + this.y*this.y + this.z*this.z);
};

vec3.prototype.normalize = function() {
    if (this.length() == 0.0) return new vec3({x: 0.0, y: 0.0, z: 1.0});
    return this.scale(1.0/this.length());
};

vec3.prototype.add = function(other) {
    return new vec3({
	x: this.x + other.x,
	y: this.y + other.y,
	z: this.z + other.z
    });
};

vec3.prototype.rotateAroundAxis = function(axis, amt) {
    var v1 = this.scale(Math.cos(amt));
    var v2 = axis.cross(this).scale(Math.sin(amt));
    var v3 = axis.scale(this.dot(axis)).scale(1.0-Math.cos(amt));
    return v1.add(v2).add(v3);
};
