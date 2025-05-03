import { createSlice } from '@reduxjs/toolkit';

export const channelColors = [
  "#EF4444", "#F97316", "#EAB308", "#22C55E",
  "#14B8A6", "#3B82F6", "#6366F1", "#A855F7"
];

const initChannels = () => {
  return Object.fromEntries(
    Array.from({ length: channelColors.length }, (_, i) => "ch" + i)
      .map((channel, i) => [channel, {
        name: "Channel " + i,
        color: channelColors[i],
        enabled: true,
        dataPoints: [22+i, 23+i, 24+i, 25+i, 26+i, 27+i, 28+i, 29+i, 30+i, 31+i, 32+i, 33+i],
      }])
  );
};

export const appSlice = createSlice({
  name: 'app',
  initialState: {
    value: 0,
    navState: 'home',
    channels: initChannels(),
  },
  reducers: {
    increment: (state) => {
      state.value += 1
    },
    decrement: (state) => {
      state.value -= 1
    },
    incrementByAmount: (state, action) => {
      state.value += action.payload
    },
    setNavState: (state, action) => {
      state.navState = action.payload
    },
    setChannels: (state, action) => {
      state.channels = action.payload
    },
  },
});

export const { increment, decrement, incrementByAmount, setNavState, setChannels } = appSlice.actions;
export const selectNavState = (state) => state.app.navState;
export const selectChannels = (state) => state.app.channels;

export default appSlice.reducer;