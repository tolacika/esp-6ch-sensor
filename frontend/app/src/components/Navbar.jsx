import { useDispatch, useSelector } from "react-redux";
import { selectNavState, setNavState } from "../store/appSlice";
import { IconFilter, IconSettings, IconTemperatureSun} from "./svgs";

import "./Navbar.css";

function NavItem({ children, isActive, onClick }) {
  return (
    <li
      className={`nav-item flex-auto ${isActive ? "active" : ""}`}
      onClick={onClick}
    >
      {children}
    </li>
  );
}

function FaIcon({ icon, alt }) {
  return <img src={icon} alt={alt} className="w-6 h-6 mr-2 inline-block" />;
}

function Navbar() {
  const navState = useSelector(selectNavState);
  const dispatch = useDispatch();
  const handleNavClick = (state) => {
    dispatch(setNavState(state));
  };

  return (
    <nav className="flex bg-gray-800 text-white top-0 p-3 flex-wrap justify-between z-20">
      <h1 className="text-lg font-semibold nav-item" onClick={() => handleNavClick("home")}>
        <IconTemperatureSun />
        ESP32 Temperature Monitor
      </h1>
      <ul className="flex gap-4 text-lg">
        <NavItem isActive={navState === "filter"} onClick={() => handleNavClick("filter")}>
          <IconFilter />
          Filter
        </NavItem>
        <NavItem isActive={navState === "contact"} onClick={() => handleNavClick("contact")}>
          <IconSettings />
          Settings
        </NavItem>
      </ul>
    </nav>
  );
}

export default Navbar;