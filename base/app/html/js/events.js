function parseSOAPResponse(xmlString) {
    // Parse the XML string into a DOM object
    const parser = new DOMParser();
    const xmlDoc = parser.parseFromString(xmlString, "application/xml");

    // Check for parsing errors
    const parseError = xmlDoc.getElementsByTagName("parsererror");
    if (parseError.length > 0) {
        console.error("Error parsing XML:", parseError[0].textContent);
        return [];
    }

    const result = [];

    // Recursive function to traverse topics
    function traverseTopics(element, parentNamespace = null, topicHierarchy = []) {
        for (const child of element.children) {
            // Skip events marked with isApplicationData="true"
            if (child.getAttribute("isApplicationData") === "true") {
                continue;
            }

            // Extract namespace and tag from the element's tag name
            const tagParts = child.tagName.split(":");
            let namespace, tagName;

            if (tagParts.length === 2) {
                namespace = tagParts[0]; // Extract namespace from the tag
                tagName = tagParts[1];  // Extract tag name
            } else if (tagParts.length === 1) {
                namespace = parentNamespace; // Inherit namespace from parent
                tagName = tagParts[0];      // Use the single part as the tag name
            }

            // Construct current topic with namespace and tag
            const currentTopic = {
                [namespace || ""]: tagName,
            };

            const newHierarchy = [...topicHierarchy, currentTopic];

            // If this element is a topic, add it to the result
            if (child.getAttribute("wstop:topic") === "true") {
                const niceName =
                    child.getAttribute("aev:NiceName") ||
                    `${topicHierarchy[topicHierarchy.length - 1]?.[namespace || ""]}:${tagName}`;
                const eventObject = { name: niceName };

                // Add topic hierarchy to event object
                newHierarchy.forEach((topic, index) => {
                    eventObject[`topic${index}`] = topic;
                });

				// Ignore events with specific criteria
				if (
					eventObject.topic0 &&
					(
						eventObject.topic0.tns1 === "UserAlarm" || 
						eventObject.topic0.tnsaxis === "AudioControl" || 
						eventObject.topic0.tnsaxis === "MediaClip" ||
						eventObject.topic0.tnsaxis === "RecordingConfig" ||
						eventObject.topic0.tns1 === "RecordingConfig" ||
						eventObject.topic0.tnsaxis === "Audio Control"	||
						eventObject.topic0.tnsxais === "MQTT" ||
						eventObject.topic0.tnsaxis === "Storage" ||
						eventObject.topic0.tnsaxis === "PTZController"
					)
				) {
					continue;
				}

				if (
					eventObject.topic1 &&
					(
						eventObject.topic1.tnsaxis === "Network" ||
						eventObject.topic1.tnsaxis === "SystemMessage" ||
						eventObject.topic1.tnsaxis === "Status" ||						
						eventObject.topic1.tnsaxis === "Alert" || 
						eventObject.topic1.tnsaxis === "Disruption" ||
						eventObject.topic1.tnsaxis === "HardwareFailure" ||
						eventObject.topic1.tns1 === "HardwareFailure" ||
						eventObject.topic1.tnsaxis === "MQTT" || 
						eventObject.topic1.tnsaxis === "RingPowerLimitExceeded" ||
						eventObject.topic1.tnsaxis === "PTZError" || 
						eventObject.topic1.tnsaxis === "ABR" ||
						eventObject.topic1.tnsaxis === "PTZReady"
					)
				) {
					continue;
				}

				if (
					eventObject.topic2 &&
					(
						eventObject.topic2.tns1 === "SystemReady"
					)
				) {
					continue;
				}


                result.push(eventObject);
            }



            // Recursively process child elements
            traverseTopics(child, namespace, newHierarchy);
        }
    }

    // Locate <SOAP-ENV:Body>
    const body = xmlDoc.getElementsByTagName("SOAP-ENV:Body")[0];
    if (!body) {
        console.error("No <SOAP-ENV:Body> found in XML.");
        return [];
    }

    // Locate <aev:GetEventInstancesResponse>
    const response = body.getElementsByTagName("aev:GetEventInstancesResponse")[0];
    if (!response) {
        console.error("No <aev:GetEventInstancesResponse> found in XML.");
        return [];
    }

    // Locate <wstop:TopicSet>
    const topicSet = response.getElementsByTagName("wstop:TopicSet")[0];
    if (!topicSet) {
        console.error("No <wstop:TopicSet> found in XML.");
        return [];
    }

    // Start traversing from <wstop:TopicSet>
    traverseTopics(topicSet);

    console.log("Parsed events:", result);
    return result;
}

function listEvents() {
    const soapRequest = `<?xml version="1.0" encoding="utf-8"?>
        <soap:Envelope xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance" 
                      xmlns:xsd="http://www.w3.org/2001/XMLSchema" 
                      xmlns:aev="http://www.axis.com/vapix/ws/event1" 
                      xmlns:wstop="http://docs.oasis-open.org/wsn/t-1" 
                      xmlns:soap="http://www.w3.org/2003/05/soap-envelope">
            <soap:Body>
                <aev:GetEventInstances xmlns="http://www.axis.com/vapix/ws/event1">
                </aev:GetEventInstances>
            </soap:Body>
        </soap:Envelope>`;

    return fetch('/vapix/services', {
        method: 'POST',
        headers: {
            'Content-Type': 'application/soap+xml; charset=utf-8',
        },
        body: soapRequest,
    })
        .then(response => {
            if (!response.ok) {
                throw new Error(`HTTP error! status: ${response.status}`);
            }
            return response.text();
        })
        .then(xmlString => parseSOAPResponse(xmlString))
        .catch(error => {
            console.error('Error fetching events:', error);
            return [];
        });
}
