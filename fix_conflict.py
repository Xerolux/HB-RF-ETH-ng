with open('src/webui.cpp', 'r') as f:
    content = f.read()

# Fix Conflict 1
block1_head = """<<<<<<< HEAD
    switch (_radioModuleDetector->getRadioModuleType()) {
        case RADIO_MODULE_HM_MOD_RPI_PCB:
            cJSON_AddStringToObject(sysinfo, "radioModuleType", "HM-MOD-RPI-PCB");
            break;
        case RADIO_MODULE_RPI_RF_MOD:
            cJSON_AddStringToObject(sysinfo, "radioModuleType", "RPI-RF-MOD");
            break;
        default:
            cJSON_AddStringToObject(sysinfo, "radioModuleType", "-");
            break;
=======
    // Determine Radio Module Type String
    const char* radioModuleTypeStr = "-";
    switch (_radioModuleDetector->getRadioModuleType())
    {
    case RADIO_MODULE_HM_MOD_RPI_PCB:
        radioModuleTypeStr = "HM-MOD-RPI-PCB";
        break;
    case RADIO_MODULE_RPI_RF_MOD:
        radioModuleTypeStr = "RPI-RF-MOD";
        break;
    default:
        break;
>>>>>>> origin/main"""

block1_res = """    // Determine Radio Module Type String
    const char* radioModuleTypeStr = "-";
    switch (_radioModuleDetector->getRadioModuleType())
    {
    case RADIO_MODULE_HM_MOD_RPI_PCB:
        radioModuleTypeStr = "HM-MOD-RPI-PCB";
        break;
    case RADIO_MODULE_RPI_RF_MOD:
        radioModuleTypeStr = "RPI-RF-MOD";
        break;
    default:
        break;
    }"""

content = content.replace(block1_head, block1_res)

# Fix Conflict 2
block2_head = """<<<<<<< HEAD
void WebUI::stop() { httpd_stop(_httpd_handle); }
=======
void WebUI::stop()
{
    httpd_stop(_httpd_handle);
}
>>>>>>> origin/main"""

block2_res = """void WebUI::stop()
{
    httpd_stop(_httpd_handle);
}"""

content = content.replace(block2_head, block2_res)

with open('src/webui.cpp', 'w') as f:
    f.write(content)
