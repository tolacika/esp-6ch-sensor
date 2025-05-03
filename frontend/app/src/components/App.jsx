import { useState } from 'react'
import CanvasJSReact from '@canvasjs/react-charts';
import { useDispatch, useSelector } from 'react-redux';
import { selectChannels, setChannels } from '../store/appSlice';
const CanvasJS = CanvasJSReact.CanvasJS;
const CanvasJSChart = CanvasJSReact.CanvasJSChart;

const formatIntToSeconds = (int) => {
  let str = "";
  str = int % 60 + " s";
  int = Math.floor(int / 60);
  if (int > 0) {
    str = int % 60 + " m " + str;
    int = Math.floor(int / 60);
    if (int > 0) {
      str = int % 24 + " h " + str;
      int = Math.floor(int / 24);
      if (int > 0) {
        str = int + " d " + str;
      }
    }
  }
  return str;
};

function App() {
  const channels = useSelector(selectChannels);
  const dispatch = useDispatch();

  let currentUnixTime1 = Math.floor(Date.now() / 1000);
  let currentUnixTime2 = Math.floor(Date.now() / 1000);

  const options = {
    animationEnabled: true,
    theme: "light2",
    axisX: {
      //valueFormatString,
      //intervalType: "second",
      //interval: 1
      title: "Time",
      labelFormatter: function (e) {
        return formatIntToSeconds(e.value);
      },
    },
    axisY: {
      title: "Temperature",
      suffix: "°C"
    },
    toolTip: {
      shared: true,
      contentFormatter: function (e) {
        let str = "";
        let dataPoint = e.entries[0].dataPoint;
        str += "<strong>T+ " + formatIntToSeconds(dataPoint.x) + "</strong><br/>";
        for (let i = 0; i < e.entries.length; i++) {
          str += "<span style='color:" + e.entries[i].dataSeries.color + "'>" +
            e.entries[i].dataSeries.name + "</span>: " +
            "<strong>" + e.entries[i].dataPoint.y + "°C</strong><br/>";
        }
        return str;
      }
    },
    legend: {
      cursor: "pointer",
      itemclick: function (e) {
        console.log({ e });
        if (typeof (e.dataSeries.visible) === "undefined" || e.dataSeries.visible) {
          e.dataSeries.visible = false;
        } else {
          e.dataSeries.visible = true;
        }
        e.chart.render();
      }
    },
    data: Object.entries(channels).map(([key, channel], i) => ({
      type: "spline",
      name: channel.name,
      showInLegend: true,
      color: channel.color,
      yValueFormatString: "#,###°C",
      dataPoints: channel.dataPoints.map((point) => ({
        y: point
      }))
    })),
    /*data: [{
      type: "spline",
      name: "Minimum",
      showInLegend: true,
      //xValueType: "dateTime",
      //xValueFormatString: valueFormatString,
      yValueFormatString: "#,###°C",
      dataPoints: [
        { y: 27 },
        { y: 28 },
        { y: 30 },
        { y: 32 },
        { y: 35 },
        { y: 37 },
        { y: 40 },
        { y: 42 },
        { y: 45 },
        { y: 48 },
        { y: 50 },
        { y: 52 },
      ]
    },
    {
      type: "spline",
      name: "Maximum",
      showInLegend: true,
      //xValueType: "dateTime",
      //xValueFormatString: valueFormatString,
      yValueFormatString: "#,###°C",
      dataPoints: [
        { y: 30 },
        { y: 32 },
        { y: 34 },
        { y: 36 },
        { y: 39 },
        { y: 41 },
        { y: 44 },
        { y: 46 },
        { y: 49 },
        { y: 52 },
        { y: 54 },
        { y: 56 }
      ]
    }]*/
  };

  const addDataPoint = () => {
    const newDataPoint = Math.floor(Math.random() * 100);
    const newChannels = JSON.parse(JSON.stringify(channels)); // Deep copy
    console.log({ newChannels, newDataPoint, channels });
    Object.entries(newChannels).forEach(([key, channel], j) => {
      channel.dataPoints = [
        ...channel.dataPoints,
        newDataPoint + j
      ];
    });

    dispatch(setChannels(newChannels));
  }

  return (
    <div className='z-10'>
      <CanvasJSChart options={options}
        className="mt-4"
        /* onRef = {ref => this.chart = ref} */
      />

      <div className="mt-4">
        <button
          type='button'
          className="bg-blue-500 text-white px-4 py-2 rounded"
          onClick={addDataPoint}
        >
          Add Data Point
        </button>
      </div>
    </div>
  );

}

export default App
