-- Called when the plugin is loaded
function Initialize()
    print("Plugin Test")
end

-- Call each frame
function UIRenderer()
    UIBegin("Lua")
    UIText("Hello, World!")
    UIEnd()
end

-- Called when a chat message is received
function OnChat(message)
    if type(message) ~= "string" then
        error("Expected a string as parameter")
    end
    print("Received: " .. message)
    return message
end

-- Call before UIRenderer
function PreRenderer()
    print("PreRender")
end