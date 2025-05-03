import App from "./App";
import Navbar from "./Navbar";
import Sidebar from "./Sidebar";


function Layout({ children }) {
  return (
    <div className="flex flex-col min-h-screen">
      <Navbar />
      <Sidebar />
      <App />
    </div>
  );
};

export default Layout;