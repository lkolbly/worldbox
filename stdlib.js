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
