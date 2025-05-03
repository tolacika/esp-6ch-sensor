import { useDispatch, useSelector } from "react-redux";
import { selectNavState, setNavState } from "../store/appSlice";
import { IconClose, IconFilter } from "./svgs";

function Sidebar() {
  const navState = useSelector(selectNavState);
  const dispatch = useDispatch();
  const isOpen = navState === "filter";
  console.log({ navState, isOpen });

  return (
    <div
      className={`fixed top-0 right-0 h-full bg-gray-700 text-white shadow-lg transform transition-transform duration-300 z-20 ${isOpen ? "translate-x-0" : "translate-x-full"
        }`}
      style={{ width: "300px" }}
    >
      <h2 className="text-xl font-bold p-4">
        <IconFilter className="inline-block mr-2 fill-white w-6 h-6" />
        Filter Options
        <IconClose
          className="inline-block float-right cursor-pointer w-6 h-6 fill-white"
          onClick={() => dispatch(setNavState("home"))}
        />
      </h2>
      <ul className="p-4 space-y-2">
        <li className="hover:bg-gray-700 p-2 rounded">Link 1</li>
        <li className="hover:bg-gray-700 p-2 rounded">Link 2</li>
        <li className="hover:bg-gray-700 p-2 rounded">Link 3</li>
      </ul>
    </div>
  );
}

export default Sidebar;