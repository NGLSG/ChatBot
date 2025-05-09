function Initialize()
    print("Plugin Test")
end

function UIRenderer()
    UIBegin("Lua")
    UIText("Hello, World!")
    UIEnd()
end

function OnChat(message)
    if type(message) ~= "string" then
        error("Expected a string as parameter")
    end
    print("Received: " .. message)
end