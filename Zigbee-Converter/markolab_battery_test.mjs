// file: external_converters/markolab_battery_bridge.mjs

import * as exposes from 'zigbee-herdsman-converters/lib/exposes';

const e = exposes.presets;
const ea = exposes.access;

const fzLocal = {
    battery_analog_inputs: {
        cluster: 'genAnalogInput',
        type: ['attributeReport', 'readResponse'],
        convert: (model, msg, publish, options, meta) => {
            const endpoint = msg.endpoint.ID;
            const value = msg.data.presentValue;

            if (value === undefined) return;

            if (endpoint === 10) {
                return {soc: Number(value.toFixed(1))};
            }
            if (endpoint === 11) {
                return {battery_voltage: Number(value.toFixed(2))};
            }
            if (endpoint === 12) {
                return {battery_current: Number(value.toFixed(2))};
            }
            if (endpoint === 13) {
                return {battery_power: Number(value.toFixed(1))};
            }
        },
    },

    battery_analog_outputs: {
        cluster: 'genAnalogOutput',
        type: ['attributeReport', 'readResponse'],
        convert: (model, msg, publish, options, meta) => {
            const endpoint = msg.endpoint.ID;
            const value = msg.data.presentValue;

            if (value === undefined) return;

            if (endpoint === 14) {
                return {grid_setpoint: Number(value.toFixed(1))};
            }
            if (endpoint === 15) {
                return {max_feedin_power: Number(value.toFixed(1))};
            }
        },
    },
};

const tzLocal = {
    grid_setpoint: {
        key: ['grid_setpoint'],
        convertSet: async (entity, key, value, meta) => {
            const num = Number(value);
            await meta.device.getEndpoint(14).write('genAnalogOutput', {presentValue: num});
            return {state: {grid_setpoint: num}};
        },
        convertGet: async (entity, key, meta) => {
            await meta.device.getEndpoint(14).read('genAnalogOutput', ['presentValue']);
        },
    },

    max_feedin_power: {
        key: ['max_feedin_power'],
        convertSet: async (entity, key, value, meta) => {
            const num = Number(value);
            await meta.device.getEndpoint(15).write('genAnalogOutput', {presentValue: num});
            return {state: {max_feedin_power: num}};
        },
        convertGet: async (entity, key, meta) => {
            await meta.device.getEndpoint(15).read('genAnalogOutput', ['presentValue']);
        },
    },
};

export default {
    fingerprint: [
        {modelID: 'ESP32C6_Battery_Bridge', manufacturerName: 'MarkoLab'},
        {modelID: 'ESP32C6_Battery_Test', manufacturerName: 'MarkoLab'},
    ],
    model: 'ESP32C6_Battery_Bridge',
    vendor: 'MarkoLab',
    description: 'ESP32-C6 battery telemetry bridge',

    fromZigbee: [
        fzLocal.battery_analog_inputs,
        fzLocal.battery_analog_outputs,
    ],

    toZigbee: [
        tzLocal.grid_setpoint,
        tzLocal.max_feedin_power,
    ],

    exposes: [
        e.numeric('soc', ea.STATE)
            .withUnit('%')
            .withDescription('Battery state of charge'),

        e.numeric('battery_voltage', ea.STATE)
            .withUnit('V')
            .withDescription('Battery voltage'),

        e.numeric('battery_current', ea.STATE)
            .withUnit('A')
            .withDescription('Battery current'),

        e.numeric('battery_power', ea.STATE)
            .withUnit('W')
            .withDescription('Battery power'),

        e.numeric('grid_setpoint', ea.ALL)
            .withUnit('W')
            .withDescription('Grid setpoint')
            .withValueMin(-100)
            .withValueMax(300),

        e.numeric('max_feedin_power', ea.ALL)
            .withUnit('W')
            .withDescription('Maximum grid feed-in power')
            .withValueMin(0)
            .withValueMax(800),
    ],
};